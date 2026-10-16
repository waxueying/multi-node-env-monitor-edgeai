#include "BAIFA_OTA.h"
#include "secrets.h"
#include "ota_flag.h"
#include "OTA.h"
#include "ESP8266.h"
#include "main.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern UART_HandleTypeDef huart2;
extern uint8_t buff2[2048];
extern uint8_t  g_ota_cap[1536];
extern volatile uint16_t g_ota_cap_len;
extern volatile uint8_t  g_ota_cap_done;
#define OTA_CAP_LEN 1536

#define HTTP_HOST "bin.bemfa.com"
#define HTTP_PORT "80"

static const char *s_err = "none";

/* set from the esp rx hook or by hand to trigger one ota run */
volatile uint8_t g_baifa_ota_request = 0;
static uint8_t s_img[OTA_CHUNK_SIZE];

static void esp_raw(const char *s)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), HAL_MAX_DELAY);
}

static uint32_t find_body(const uint8_t *b, uint32_t len)
{
    uint32_t i;

    for (i = 0; i + 3 < len; i++)
    {
        if (b[i] == 0x0D && b[i + 1] == 0x0A && b[i + 2] == 0x0D && b[i + 3] == 0x0A)
        {
            return i + 4;
        }
    }
    return 0;
}

static int hdr_uint(const char *buf, const char *name, uint32_t *out)
{
    const char *p = strstr(buf, name);

    if (p == 0) return -1;
    p += strlen(name);
    while (*p == 0x20 || *p == 0x0D || *p == 0x0A) p++;
    *out = (uint32_t)strtoul(p, 0, 10);
    return 0;
}

static int hdr_quoted(const char *buf, const char *name, char *out, uint32_t out_len)
{
    const char *p = strstr(buf, name);
    uint32_t i = 0;

    if (p == 0) return -1;
    p += strlen(name);
    while (*p != 0 && *p != 0x22) p++;
    if (*p == 0x22) p++;
    while (*p != 0 && *p != 0x22 && *p != 0x0D && *p != 0x0A && i + 1 < out_len)
    {
        out[i++] = *p++;
    }
    out[i] = 0;
    return (i > 0) ? 0 : -1;
}

static int ver_cmp(const char *remote, const char *local)
{
    int r[3] = {0, 0, 0};
    int l[3] = {0, 0, 0};
    int i;

    sscanf(remote, "%d.%d.%d", &r[0], &r[1], &r[2]);
    sscanf(local, "%d.%d.%d", &l[0], &l[1], &l[2]);
    for (i = 0; i < 3; i++)
    {
        if (r[i] > l[i]) return 1;
        if (r[i] < l[i]) return 0;
    }
    return 0;
}

static int at_cmd(const char *cmd, const char *resp, uint32_t timeout_ms)
{
    uint32_t t0;

    memset(buff2, 0, 2048);
    esp_raw(cmd);
    t0 = HAL_GetTick();
    while ((HAL_GetTick() - t0) < timeout_ms)
    {
        if (strstr((char *)buff2, resp) != 0) return 1;
        osDelay(20);
    }
    return 0;
}


/* -------- flash helpers for slot B -------- */

static int slot_erase(void)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t err = 0;

    HAL_FLASH_Unlock();
    erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    erase.Sector       = OTA_SLOT_SECTOR;
    erase.NbSectors    = 1;
    if (HAL_FLASHEx_Erase(&erase, &err) != HAL_OK)
    {
        HAL_FLASH_Lock();
        printf("[OTA] slot erase failed %lu\r\n", (unsigned long)err);
        return -1;
    }
    HAL_FLASH_Lock();
    return 0;
}

static int slot_write(uint32_t offset, const void *data, uint32_t len)
{
    const uint32_t *src = (const uint32_t *)data;
    uint32_t words = (len + 3U) / 4U;
    uint32_t i;

    HAL_FLASH_Unlock();
    for (i = 0; i < words; i++)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, OTA_APP_B_ADDR + offset + i * 4U, src[i]) != HAL_OK)
        {
            HAL_FLASH_Lock();
            printf("[OTA] slot write failed @ %lu\r\n", (unsigned long)(offset + i * 4U));
            return -1;
        }
    }
    HAL_FLASH_Lock();
    return 0;
}

/* vector table sanity check for the freshly written slot B */
static int image_ok(void)
{
    uint32_t sp = *(volatile uint32_t *)OTA_APP_B_ADDR;
    uint32_t pc = *(volatile uint32_t *)(OTA_APP_B_ADDR + 4U);

    if (sp < 0x20000000UL || sp > 0x20020000UL) return 0;
    if (pc < 0x08000000UL || pc > 0x08080000UL) return 0;
    if ((pc & 1U) == 0) return 0;
    return 1;
}

static int http_download(const char *url, uint32_t size)
{
    const char *host = strstr(url, "://");
    const char *path;
    char hostonly[64];
    uint32_t off = 0;
    char cmd[240];

    if (host == 0) return -1;
    host += 3;
    path = strchr(host, 0x2F);
    if (path == 0) return -1;
    {
        uint32_t n = (uint32_t)(path - host);
        if (n > 63) n = 63;
        memcpy(hostonly, host, n);
        hostonly[n] = 0;
    }
    path++;
    printf("[OTA] host=%s size=%lu\r\n", hostonly, (unsigned long)size);

    sprintf(cmd, "AT+CIPSTART=0,\"TCP\",\"%s\",\"80\"\r\n", hostonly);
    if (!ESP_Send_Cmd(cmd, "OK", 12000))
    {
        s_err = "cipstart";
        return -1;
    }

    while (off < size)
    {
        uint32_t want = (size - off > OTA_CHUNK_SIZE) ? OTA_CHUNK_SIZE : (size - off);
        uint32_t end = off + want - 1U;
        uint32_t bs, t0;
        int got = 0;

        memset(cmd, 0, sizeof(cmd));
        sprintf(cmd, "GET /%s HTTP/1.1\r\nHost: %s\r\nRange: bytes=%lu-%lu\r\nConnection: close\r\n\r\n",
                path, hostonly, (unsigned long)off, (unsigned long)end);

        {
            char sd[16];

            sprintf(sd, "AT+CIPSEND=0,%u\r\n", (unsigned)strlen(cmd));
            if (!ESP_Send_Cmd(sd, ">", 6000))
            {
                s_err = "cipsend";
                break;
            }
        }

        memset(buff2, 0, 2048);
        esp_raw(cmd);

        t0 = HAL_GetTick();
        while ((HAL_GetTick() - t0) < 20000)
        {
            bs = find_body(g_ota_cap, g_ota_cap_len);
            if (bs > 0 && ((uint32_t)g_ota_cap_len - bs) >= 200)
            {
                uint32_t k = 0;

                while (k < want && (bs + k) < (uint32_t)g_ota_cap_len)
                {
                    s_img[k] = g_ota_cap[bs + k];
                    k++;
                }
                if (k >= want)
                {
                    got = 1;
                }
            }
            if (got) break;
            osDelay(20);
        }

        if (!got)
        {
            s_err = "http body";
            break;
        }
        if (slot_write(off, s_img, want) != 0)
        {
            s_err = "flash";
            break;
        }
        off += want;
        printf("[OTA] %lu/%lu\r\n", (unsigned long)off, (unsigned long)size);
    }

    ESP_Send_Cmd("AT+CIPCLOSE=0\r\n", "OK", 3000);
    return (off >= size) ? 0 : -1;
}

int Baifa_OTA_Check_Version(baifa_version_t *info)
{
    char resp[1024];
    char url[160];
    uint32_t api_size = 0;

    if (info == 0) return BAIFA_OTA_ERR_VERSION;

    s_err = "version api";
    memset(resp, 0, sizeof(resp));
    if (!ESP_Send_Cmd("AT+CIPSTART=0,\"TCP\",\"apis.bemfa.com\",\"80\"\r\n", "OK", 12000))
    {
        return BAIFA_OTA_ERR_ESP;
    }
    memset(buff2, 0, 2048);
    ESP_Send_Cmd("GET /vb/api/v1/firmwareVersion?openID=" OTA_BEMFA_KEY "&topic=" OTA_BEMFA_TOPIC "&deviceType=1 HTTP/1.1\r\nHost: apis.bemfa.com\r\nConnection: close\r\n\r\n", "version", 8000);    osDelay(2000);
    memcpy(resp, buff2, 1000);
    resp[1000] = 0;
    ESP_Send_Cmd("AT+CIPCLOSE=0\r\n", "OK", 3000);

    if (hdr_quoted(resp, "\"version\":", info->version, sizeof(info->version)) != 0)
    {
        s_err = "version parse";
        return BAIFA_OTA_ERR_VERSION;
    }
    if (hdr_uint(resp, "\"size\":", &api_size) != 0)
    {
        s_err = "size parse";
        return BAIFA_OTA_ERR_VERSION;
    }
    info->size = api_size;

    return BAIFA_OTA_OK;
}

static int url_quote_value(const char *buf, char *out, uint32_t out_len)
{
    const char *p = strstr(buf, "bin.bemfa.com");
    uint32_t i = 0;

    if (p == 0) return -1;
    while (p > buf && *(p - 1) != 0x22) p--;
    while (*p != 0 && *p != 0x22 && i + 1 < out_len)
    {
        out[i++] = *p++;
    }
    out[i] = 0;
    return (i > 0) ? 0 : -1;
}

int Baifa_OTA_Start(void)
{
    baifa_version_t vi;
    char resp[1024];
    char url[160];
    char full[200];

    memset(&vi, 0, sizeof(vi));
    memset(&resp, 0, sizeof(resp));
    memset(&url, 0, sizeof(url));

    s_err = "version api";
    if (!ESP_Send_Cmd("AT+CIPSTART=0,\"TCP\",\"apis.bemfa.com\",\"80\"\r\n", "OK", 12000))
    {
        return BAIFA_OTA_ERR_ESP;
    }
    memset(buff2, 0, 2048);
    ESP_Send_Cmd("GET /vb/api/v1/firmwareVersion?openID=" OTA_BEMFA_KEY "&topic=" OTA_BEMFA_TOPIC "&deviceType=1 HTTP/1.1\r\nHost: apis.bemfa.com\r\nConnection: close\r\n\r\n", "version", 8000);    osDelay(2000);
    memcpy(resp, buff2, 1000);
    resp[1000] = 0;
    ESP_Send_Cmd("AT+CIPCLOSE=0\r\n", "OK", 3000);

    if (hdr_quoted(resp, "\"version\":", vi.version, sizeof(vi.version)) != 0)
    {
        s_err = "version parse";
        return BAIFA_OTA_ERR_VERSION;
    }
    if (hdr_uint(resp, "\"size\":", &vi.size) != 0)
    {
        s_err = "size parse";
        return BAIFA_OTA_ERR_VERSION;
    }

    printf("[OTA] cloud=%s local=%s size=%lu\r\n", vi.version, MCU_VER, (unsigned long)vi.size);

    if (!ver_cmp(vi.version, MCU_VER))
    {
        s_err = "no new version";
        return BAIFA_OTA_ERR_VERSION;
    }

    if (url_quote_value(resp, url, sizeof(url)) != 0)
    {
        s_err = "url parse";
        return BAIFA_OTA_ERR_VERSION;
    }
    sprintf(full, "http://%s", url);
    printf("[OTA] download %s\r\n", full);

    if (vi.size == 0 || vi.size > 120U * 1024U)
    {
        s_err = "size out of slot";
        return BAIFA_OTA_ERR_IMAGE;
    }

    s_err = "erase slot";
    if (slot_erase() != 0)
    {
        return BAIFA_OTA_ERR_FLASH;
    }

    s_err = "download";
    if (http_download(full, vi.size) != 0)
    {
        return BAIFA_OTA_ERR_HTTP;
    }

    s_err = "image check";
    if (!image_ok())
    {
        return BAIFA_OTA_ERR_IMAGE;
    }

    printf("[OTA] image ok, set pending -> B\r\n");
    if (ota_flag_set_pending(OTA_SLOT_B) != 0)
    {
        s_err = "flag write";
        return BAIFA_OTA_ERR_FLASH;
    }

    s_err = "none";
    return BAIFA_OTA_OK;
}

void Baifa_OTA_Init(void)
{
    s_err = "none";
}

const char *Baifa_OTA_Last_Error(void)
{
    return s_err;
}

void Baifa_OTA_Dump_Flag(void)
{
    ota_flag_t f;
    uint32_t me = ota_flag_my_slot();

    printf("running slot %c, vtor=0x%08lx\r\n",
           (me == OTA_SLOT_B) ? 0x42 : 0x41,
           (unsigned long)(*(volatile uint32_t *)0xE000ED08UL));
    if (ota_flag_read(&f) == 1)
    {
        printf("flag: active=%c pending=%c\r\n",
               (f.active == OTA_SLOT_B) ? 0x42 : 0x41,
               (f.pending == 0) ? 0x2D : ((f.pending == OTA_SLOT_B) ? 0x42 : 0x41));
    }
    else
    {
        printf("flag: blank/invalid (bootloader defaults to A)\r\n");
    }
}
