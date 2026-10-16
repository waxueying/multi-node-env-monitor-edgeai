#include "wifi_ota.h"
#include "wifi_ota_config.h"

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"

#define DBG_TAG "OTA"

#define WIFI_BIT_READY  BIT0
#define OTA_RESP_MAX    768
#define OTA_DL_BUF_SIZE 4096

static EventGroupHandle_t s_wifi_evt   = NULL;
static char               s_ip[16]     = {0};
static bool               s_wifi_ready = false;

/* ------------------------------------------------------------------ WiFi 事件 */
/* 事件回调里只记录状态，重连放到 wifi_ota_task 里做 —— 回调里不适合阻塞重试 */
static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_event_sta_disconnected_t *d = (wifi_event_sta_disconnected_t *)data;

        s_wifi_ready = false;
        if (s_wifi_evt) xEventGroupClearBits(s_wifi_evt, WIFI_BIT_READY);
        ESP_LOGW(DBG_TAG, "WiFi 断开 (reason=%d)", d->reason);
    }
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;

        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&e->ip_info.ip));
        s_wifi_ready = true;
        if (s_wifi_evt) xEventGroupSetBits(s_wifi_evt, WIFI_BIT_READY);
    }
}

/* ------------------------------------------------------------------ 连 WiFi */
esp_err_t wifi_ota_wifi_connect(void)
{
    esp_err_t      err;
    wifi_config_t  wc = {0};

    /* WiFi 驱动要用 nvs 存射频校准数据 */
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) return err;

    s_wifi_evt = xEventGroupCreate();
    if (s_wifi_evt == NULL) return ESP_ERR_NO_MEM;

    /* 这两步重复调用会返回 ESP_ERR_INVALID_STATE，这里当正常情况放过 */
    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, NULL, NULL));

    snprintf((char *)wc.sta.ssid, sizeof(wc.sta.ssid), "%s", WIFI_SSID);
    snprintf((char *)wc.sta.password, sizeof(wc.sta.password), "%s", WIFI_PASSWORD);
    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(DBG_TAG, "正在连接 WiFi: %s ...", WIFI_SSID);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_evt, WIFI_BIT_READY, pdFALSE, pdTRUE,
                                           pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));
    if (bits & WIFI_BIT_READY)
    {
        ESP_LOGI(DBG_TAG, "WiFi 已连接: IP=%s", s_ip);
        return ESP_OK;
    }

    ESP_LOGE(DBG_TAG, "WiFi 连接超时(%d ms)：检查 SSID/密码，以及路由器是否开了 2.4G",
             WIFI_CONNECT_TIMEOUT_MS);
    return ESP_ERR_TIMEOUT;
}

/* ------------------------------------------------------------------ 收 HTTP 响应 */
typedef struct
{
    char buf[OTA_RESP_MAX];
    int  len;
} ota_http_resp_t;

static esp_err_t ota_http_event(esp_http_client_event_t *evt)
{
    ota_http_resp_t *r = (ota_http_resp_t *)evt->user_data;

    if (evt->event_id == HTTP_EVENT_ON_DATA && r != NULL && evt->data_len > 0)
    {
        int space = (int)sizeof(r->buf) - 1 - r->len;
        int n     = (evt->data_len < space) ? evt->data_len : space;

        if (n > 0)
        {
            memcpy(r->buf + r->len, evt->data, n);
            r->len += n;
            r->buf[r->len] = '\0';
        }
    }
    return ESP_OK;
}

/* ------------------------------------------------------------------ NVS 版本记录 */
/* 用 NVS 记住"已经装过的云端版本号"，解决两个问题：
 *   ① 巴法云的版本号是每上传一次自动 +1，如果拿编译期宏去比，装完之后仍然是
 *      "云端 > 本地"，会陷入无限升级循环；
 *   ② 顺便不用再手动改版本宏。
 * 首次运行没有记录时，用编译期的 NODE1_FW_VERSION 兜底。 */
#define OTA_NVS_NAMESPACE "ota"
#define OTA_NVS_KEY_VER   "ins_ver"

static uint32_t ota_get_installed_version(void)
{
    nvs_handle_t h;
    uint32_t     v = 0;

    if (nvs_open(OTA_NVS_NAMESPACE, NVS_READONLY, &h) == ESP_OK)
    {
        if (nvs_get_u32(h, OTA_NVS_KEY_VER, &v) != ESP_OK)
        {
            v = 0;
        }
        nvs_close(h);
    }
    return v;
}

static void ota_set_installed_version(uint32_t v)
{
    nvs_handle_t h;

    if (nvs_open(OTA_NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK)
    {
        nvs_set_u32(h, OTA_NVS_KEY_VER, v);
        nvs_commit(h);
        nvs_close(h);
        ESP_LOGI(DBG_TAG, "已记录安装版本: %lu", (unsigned long)v);
    }
}

/* ------------------------------------------------------------------ 下载并切换 */
static char s_dl_buf[OTA_DL_BUF_SIZE];      /* 4KB，别放栈上 */

static esp_err_t ota_download_to(const ota_cloud_info_t *info, const esp_partition_t *part)
{
    esp_http_client_config_t cfg = {0};
    esp_http_client_handle_t cli = NULL;
    esp_ota_handle_t         ota = 0;
    esp_err_t                err;
    int                      got         = 0;
    int                      last_report = 0;

    ESP_LOGI(DBG_TAG, "开始下载: %s", info->url);
    ESP_LOGI(DBG_TAG, "目标分区: %s (偏移 0x%lx, 大小 %lu KB)",
             part->label, (unsigned long)part->address, (unsigned long)(part->size / 1024));

    cfg.url        = info->url;
    cfg.timeout_ms = 20000;

    cli = esp_http_client_init(&cfg);
    if (cli == NULL) return ESP_FAIL;

    err = esp_http_client_open(cli, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(DBG_TAG, "连接固件地址失败: %s", esp_err_to_name(err));
        esp_http_client_cleanup(cli);
        return err;
    }

    int content_len = esp_http_client_fetch_headers(cli);
    ESP_LOGI(DBG_TAG, "HTTP %d，固件大小 %d 字节",
             esp_http_client_get_status_code(cli), content_len);

    err = esp_ota_begin(part, (info->size > 0) ? (size_t)info->size : OTA_SIZE_UNKNOWN, &ota);
    if (err != ESP_OK)
    {
        ESP_LOGE(DBG_TAG, "esp_ota_begin 失败: %s", esp_err_to_name(err));
        esp_http_client_close(cli);
        esp_http_client_cleanup(cli);
        return err;
    }

    while (1)
    {
        int n = esp_http_client_read(cli, s_dl_buf, sizeof(s_dl_buf));

        if (n < 0)
        {
            err = ESP_FAIL;
            break;
        }
        if (n == 0) break;                  /* 收完 */

        err = esp_ota_write(ota, s_dl_buf, n);
        if (err != ESP_OK) break;

        got += n;
        if (got - last_report >= 131072)    /* 每 128KB 报一次进度 */
        {
            last_report = got;
            ESP_LOGI(DBG_TAG, "已下载 %d / %d 字节 (%d%%)",
                     got, info->size, (info->size > 0) ? (got * 100 / info->size) : 0);
        }
    }

    /* 连接中途断了会表现为"数据没收完"，这种情况一律丢弃 */
    if (err == ESP_OK && !esp_http_client_is_complete_data_received(cli))
    {
        ESP_LOGE(DBG_TAG, "数据没收完（连接中断）");
        err = ESP_FAIL;
    }

    esp_http_client_close(cli);
    esp_http_client_cleanup(cli);

    if (err != ESP_OK)
    {
        ESP_LOGE(DBG_TAG, "下载失败(%s)，已丢弃，继续跑旧固件", esp_err_to_name(err));
        esp_ota_abort(ota);
        return err;
    }

    /* 校验镜像：头部格式/完整性不对，esp_ota_end 会报错 */
    err = esp_ota_end(ota);
    if (err != ESP_OK)
    {
        ESP_LOGE(DBG_TAG, "镜像校验失败: %s（坏包，拒绝切换）", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(DBG_TAG, "镜像校验通过，共 %d 字节", got);
    return ESP_OK;
}

static esp_err_t wifi_ota_perform_update(const ota_cloud_info_t *info)
{
    const esp_partition_t *target;
    esp_err_t              err;

    /* IDF 自动算出"当前没在跑的那个分区"：跑在 A 区就返回 B 区，反之亦然 */
    target = esp_ota_get_next_update_partition(NULL);
    if (target == NULL)
    {
        ESP_LOGE(DBG_TAG, "找不到可写入的 OTA 分区");
        return ESP_FAIL;
    }

    err = ota_download_to(info, target);
    if (err != ESP_OK) return err;

    err = esp_ota_set_boot_partition(target);
    if (err != ESP_OK)
    {
        ESP_LOGE(DBG_TAG, "设置启动分区失败: %s", esp_err_to_name(err));
        return err;
    }

    /* 先记版本再重启：否则重启后它不知道自己装过了，会反复升级。
     * 代价：这一版如果因为新固件有问题而回滚，设备不会再自动重试这一版
     *       （要修就得传一版新的）。这是为了避免坏固件造成无限升级循环。 */
    ota_set_installed_version((uint32_t)info->version);

    ESP_LOGI(DBG_TAG, "升级准备完成，3 秒后重启切换到 %s ...", target->label);
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();

    return ESP_OK;      /* 不会执行到这里 */
}

/* ------------------------------------------------------------------ 开机确认 */
void wifi_ota_confirm_boot(void)
{
    const esp_partition_t *run = esp_ota_get_running_partition();
    esp_ota_img_states_t   st;

    if (run == NULL) return;

    /* 只有"刚升级完、待验证"的镜像需要确认；USB 烧录的镜像不是这个状态，自动跳过 */
    if (esp_ota_get_state_partition(run, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY)
    {
        if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK)
        {
            ESP_LOGI(DBG_TAG, "新固件自检通过，已确认本次升级（解除回滚保护）");
        }
        else
        {
            ESP_LOGE(DBG_TAG, "确认升级失败，下次重启会回滚！");
        }
    }
}

/* ------------------------------------------------------------------ 查版本 */
esp_err_t wifi_ota_check_version(ota_cloud_info_t *info, bool *has_new)
{
    static ota_http_resp_t   resp;          /* 768 字节，别放栈上 */
    esp_http_client_handle_t cli;
    esp_http_client_config_t cfg = {0};
    char      url[320];
    esp_err_t err;

    if (info == NULL || has_new == NULL) return ESP_ERR_INVALID_ARG;

    memset(info, 0, sizeof(*info));
    memset(&resp, 0, sizeof(resp));
    *has_new = false;

    snprintf(url, sizeof(url),
             "https://apis.bemfa.com/vb/api/v1/firmwareVersion?openID=%s&topic=%s&deviceType=%d",
             BEMFA_OPEN_ID, BEMFA_TOPIC, BEMFA_DEVICE_TYPE);

    cfg.url               = url;
    cfg.method            = HTTP_METHOD_GET;
    cfg.timeout_ms        = 10000;
    cfg.event_handler     = ota_http_event;
    cfg.user_data         = &resp;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;   /* HTTPS 证书包（sdkconfig 已开） */

    cli = esp_http_client_init(&cfg);
    if (cli == NULL)
    {
        ESP_LOGE(DBG_TAG, "http client 初始化失败");
        return ESP_FAIL;
    }

    err = esp_http_client_perform(cli);
    if (err != ESP_OK)
    {
        ESP_LOGE(DBG_TAG, "HTTP 请求失败: %s", esp_err_to_name(err));
        esp_http_client_cleanup(cli);
        return err;
    }

    ESP_LOGI(DBG_TAG, "HTTP %d，收到 %d 字节",
             esp_http_client_get_status_code(cli), resp.len);
    esp_http_client_cleanup(cli);

    /* ---- 解析 JSON ---- */
    cJSON *root = cJSON_Parse(resp.buf);
    if (root == NULL)
    {
        ESP_LOGE(DBG_TAG, "JSON 解析失败，原文: %.120s", resp.buf);
        return ESP_FAIL;
    }

    cJSON *jcode = cJSON_GetObjectItem(root, "code");
    cJSON *jdata = cJSON_GetObjectItem(root, "data");

    if (!cJSON_IsNumber(jcode) || jcode->valueint != 0 || !cJSON_IsObject(jdata))
    {
        ESP_LOGE(DBG_TAG, "接口返回异常: %.160s", resp.buf);
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    cJSON *jurl = cJSON_GetObjectItem(jdata, "url");
    cJSON *jver = cJSON_GetObjectItem(jdata, "version");
    cJSON *jsz  = cJSON_GetObjectItem(jdata, "size");

    if (cJSON_IsString(jurl))  snprintf(info->url, sizeof(info->url), "%s", jurl->valuestring);
    if (cJSON_IsNumber(jver))  info->version = jver->valueint;
    if (cJSON_IsNumber(jsz))   info->size    = jsz->valueint;

    cJSON_Delete(root);

    /* ---- 和本地版本比较 ---- */
    if (info->version == 0 && info->size == 0)
    {
        ESP_LOGW(DBG_TAG, "云端没查到固件 —— 检查 topic(%s) / deviceType(%d) 是否和巴法云上的主题一致",
                 BEMFA_TOPIC, BEMFA_DEVICE_TYPE);
        return ESP_ERR_NOT_FOUND;
    }

    /* ---- 和"已安装版本"比较（NVS 记录，首次运行用编译期宏兜底） ---- */
    uint32_t installed = ota_get_installed_version();
    if (installed == 0)
    {
        installed = NODE1_FW_VERSION;
    }

    *has_new = (info->version > (int)installed);

    ESP_LOGI(DBG_TAG, "版本对比: 云端=%d 已安装=%lu 大小=%d 字节",
             info->version, (unsigned long)installed, info->size);

    if (*has_new)
    {
        ESP_LOGI(DBG_TAG, ">>> 发现新版本! <<<");
        ESP_LOGI(DBG_TAG, "固件地址: %s", info->url);
        ESP_LOGI(DBG_TAG, "准备升级...");
    }
    else
    {
        ESP_LOGI(DBG_TAG, "已是最新版本");
    }

    return ESP_OK;
}

/* ------------------------------------------------------------------ 任务 */
void wifi_ota_task(void *arg)
{
    ota_cloud_info_t       info;
    bool                   has_new = false;
    const esp_partition_t *run;

    (void)arg;

    /* 开机先亮明身份：跑在哪个分区，一眼就能看出有没有切换成功 */
    run = esp_ota_get_running_partition();
    ESP_LOGI(DBG_TAG, "当前运行分区: %s (偏移 0x%lx)",
             (run != NULL) ? run->label : "?",
             (run != NULL) ? (unsigned long)run->address : 0UL);

    /* 先跑 5 秒再说"我好了"：新固件要是启动就崩，赶不上这次确认，
     * 下次重启 bootloader 就会自动回滚到上一版 */
    vTaskDelay(pdMS_TO_TICKS(OTA_VERIFY_DELAY_MS));
    wifi_ota_confirm_boot();

    if (wifi_ota_wifi_connect() != ESP_OK)
    {
        ESP_LOGE(DBG_TAG, "首次连接失败，稍后自动重试");
    }

    for (;;)
    {
        if (!s_wifi_ready)
        {
            ESP_LOGW(DBG_TAG, "WiFi 未连接，尝试重连...");
            esp_wifi_connect();
            xEventGroupWaitBits(s_wifi_evt, WIFI_BIT_READY, pdFALSE, pdTRUE,
                                pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));
        }

        if (s_wifi_ready)
        {
            esp_err_t err = wifi_ota_check_version(&info, &has_new);

            if (err == ESP_OK && has_new)
            {
                /* 成功的话这个函数不会返回（已经重启了）；失败只打印，等下一轮再试 */
                wifi_ota_perform_update(&info);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(OTA_CHECK_INTERVAL_MS));
    }
}

