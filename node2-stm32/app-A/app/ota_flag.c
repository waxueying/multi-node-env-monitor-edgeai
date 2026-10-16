#include "ota_flag.h"
#include "main.h"
#include <stdio.h>

/* flag sector = Sector 4 (0x08010000, 64KB). app runs in Sector 5/6, so erasing S4 is safe. */

uint32_t ota_flag_my_slot(void)
{
    uint32_t vtor = *(volatile uint32_t *)0xE000ED08UL;

    return (vtor >= 0x08040000UL) ? OTA_SLOT_B : OTA_SLOT_A;
}

int ota_flag_read(ota_flag_t *f)
{
    const ota_flag_t *p = (const ota_flag_t *)OTA_FLAG_ADDR;

    *f = *p;

    if (f->magic != OTA_FLAG_MAGIC)
    {
        return 0;
    }
    return (f->check == (f->magic ^ f->active ^ f->pending)) ? 1 : 0;
}

static int ota_flag_erase_sector(void)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t sector_error = 0;

    HAL_FLASH_Unlock();

    erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    erase.Sector       = OTA_FLAG_SECTOR;
    erase.NbSectors    = 1;

    if (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK)
    {
        HAL_FLASH_Lock();
        printf("[OTA] flag erase failed (%lu)\r\n", (unsigned long)sector_error);
        return -1;
    }

    HAL_FLASH_Lock();
    return 0;
}

static int ota_flag_program(uint32_t addr, const void *data, uint32_t len)
{
    const uint32_t *src = (const uint32_t *)data;
    uint32_t i;

    HAL_FLASH_Unlock();

    for (i = 0; i < (len / 4U); i++)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i * 4U, src[i]) != HAL_OK)
        {
            HAL_FLASH_Lock();
            printf("[OTA] flag program failed\r\n");
            return -1;
        }
    }

    HAL_FLASH_Lock();
    return 0;
}

int ota_flag_write(uint32_t active, uint32_t pending)
{
    ota_flag_t f;

    f.magic   = OTA_FLAG_MAGIC;
    f.active  = active;
    f.pending = pending;
    f.check   = f.magic ^ f.active ^ f.pending;

    if (ota_flag_erase_sector() != 0)
    {
        return -1;
    }
    if (ota_flag_program(OTA_FLAG_ADDR, &f, sizeof(f)) != 0)
    {
        return -1;
    }

    printf("[OTA] flag written: active=%c pending=%c\r\n",
           active == OTA_SLOT_B ? 'B' : 'A',
           pending == 0 ? '-' : (pending == OTA_SLOT_B ? 'B' : 'A'));
    return 0;
}

int ota_flag_confirm(void)
{
    ota_flag_t f;
    uint32_t me = ota_flag_my_slot();

    if (ota_flag_read(&f) == 1 && f.active == me && f.pending == 0)
    {
        return 0;
    }

    return ota_flag_write(me, 0);
}

int ota_flag_set_pending(uint32_t slot)
{
    ota_flag_t f;
    uint32_t active = (ota_flag_read(&f) == 1) ? f.active : ota_flag_my_slot();

    return ota_flag_write(active, slot);
}