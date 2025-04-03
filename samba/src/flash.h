#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

#include "uf2.h"
#include <stdint.h>
#include <stdbool.h>

#define FLASH_ROW_SIZE 256
#define FLASH_NUM_ROWS 1024
#define FLASH_DEVICE_SIZE 0x10000

void flash_erase_row(uint32_t *dst);
void flash_erase_nvm_user_config(void);
void flash_write_nvm_user_config(uint32_t value);
void flash_bootloader_section_lock(void);
bool flash_bootloader_section_unlock(void);
#ifdef __cplusplus
}
#endif
