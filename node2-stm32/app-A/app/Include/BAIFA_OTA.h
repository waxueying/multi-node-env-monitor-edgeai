#ifndef __BAIFA_OTA_H
#define __BAIFA_OTA_H

#include <stdint.h>

/* node2 Bemfa cloud OTA client
 *   version api -> download self2ota.bin -> write slot B -> verify -> pending -> reboot
 * flash map (512KB): boot 64KB | flag 64KB | APP-A 128KB | APP-B 128KB
 */

#define BAIFA_VERSION_API "http://apis.bemfa.com/vb/api/v1/firmwareVersion?openID=your-bemfa-private-key&topic=self2ota&deviceType=1"

#define OTA_APP_A_ADDR   0x08020000UL
#define OTA_APP_B_ADDR   0x08040000UL

#define OTA_CHUNK_SIZE   768U
#define OTA_SLOT_SECTOR  6U

typedef enum
{
    BAIFA_OTA_OK = 0,
    BAIFA_OTA_ERR_ESP,
    BAIFA_OTA_ERR_VERSION,
    BAIFA_OTA_ERR_HTTP,
    BAIFA_OTA_ERR_FLASH,
    BAIFA_OTA_ERR_IMAGE,
} baifa_ota_status_t;

typedef struct
{
    char     version[24];
    uint32_t size;
} baifa_version_t;

void Baifa_OTA_Init(void);
int  Baifa_OTA_Start(void);
int  Baifa_OTA_Check_Version(baifa_version_t *info);
const char *Baifa_OTA_Last_Error(void);
void Baifa_OTA_Dump_Flag(void);

#endif
