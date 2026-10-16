#ifndef __OTA_FLAG_H
#define __OTA_FLAG_H

#include <stdint.h>

/* OTA flag sector (bootloader/src/main.c boot_flag_t layout must match) */
/* node2 flash map 512KB F407VET6:
 *   0x08000000   64KB  bootloader   Sector 0~3
 *   0x08010000   64KB  flag sector  Sector 4
 *   0x08020000  128KB  APP-A        Sector 5
 *   0x08040000  128KB  APP-B        Sector 6
 */

#define OTA_FLAG_ADDR   0x08010000UL
#define OTA_FLAG_MAGIC  0x5A5AA5A5UL

#define OTA_SLOT_A      0x41UL
#define OTA_SLOT_B      0x42UL

#define OTA_FLAG_SECTOR 4U

typedef struct
{
    uint32_t magic;
    uint32_t active;
    uint32_t pending;
    uint32_t check;
} ota_flag_t;

uint32_t ota_flag_my_slot(void);
int ota_flag_read(ota_flag_t *f);
int ota_flag_write(uint32_t active, uint32_t pending);
int ota_flag_confirm(void);
int ota_flag_set_pending(uint32_t slot);

#endif
