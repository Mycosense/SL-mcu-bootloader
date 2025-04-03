#include "flash.h"

#define BLD_LOCKED 0x2
#define BLD_UNLOCKED 0x7

// this actually generates less code than a function
#define wait_ready()                                                                               \
    while (NVMCTRL->INTFLAG.bit.READY == 0)                                                        \
        ;

void flash_erase_row(uint32_t *dst) {
    wait_ready();
    NVMCTRL->STATUS.reg = NVMCTRL_STATUS_MASK;

    // Execute "ER" Erase Row
    NVMCTRL->ADDR.reg = (uint32_t)dst / 2;
    NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_ER;
    wait_ready();
}

void flash_erase_to_end(uint32_t *start_address) {
    // Note: the flash memory is erased in ROWS, that is in
    // block of 4 pages.
    //       Even if the starting address is the last byte
    //       of a ROW the entire
    //       ROW is erased anyway.

    uint32_t dst_addr = (uint32_t) start_address; // starting address

    while (dst_addr < FLASH_SIZE) {
        flash_erase_row((void *)dst_addr);
        dst_addr += FLASH_ROW_SIZE;
    }
}

void copy_words(uint32_t *dst, uint32_t *src, uint32_t n_words) {
    while (n_words--)
        *dst++ = *src++;
}

void flash_write_words(uint32_t *dst, uint32_t *src, uint32_t n_words) {
    // Set automatic page write
    NVMCTRL->CTRLB.bit.MANW = 0;

    while (n_words > 0) {
        uint32_t len = (FLASH_PAGE_SIZE >> 2) < n_words ? (FLASH_PAGE_SIZE >> 2) : n_words;
        n_words -= len;

        // Execute "PBC" Page Buffer Clear
        NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_PBC;
        wait_ready();

        // make sure there are no other memory writes here
        // otherwise we get lock-ups

        while (len--)
            *dst++ = *src++;

        // Execute "WP" Write Page
        NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_WP;
        wait_ready();
    }
}

void flash_write(void) {
    uint32_t *src = (void *)0x20006000;
    uint32_t *dst = (void *)*src++;
    uint32_t n_rows = *src++;

    NVMCTRL->CTRLB.bit.MANW = 1;
    while (n_rows--) {
        wait_ready();
        NVMCTRL->STATUS.reg = NVMCTRL_STATUS_MASK;

        // Execute "ER" Erase Row
        NVMCTRL->ADDR.reg = (uint32_t)dst / 2;
        NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_ER;
        wait_ready();

        // there are 4 pages to a row
        for (int i = 0; i < 4; ++i) {
            // Execute "PBC" Page Buffer Clear
            NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_PBC;
            wait_ready();

            uint32_t len = FLASH_PAGE_SIZE >> 2;
            while (len--)
                *dst++ = *src++;

            // Execute "WP" Write Page
            NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_WP;
            wait_ready();
        }
    }
}

// Skip writing blocks that are identical to the existing block.
// only disable for debugging/timing
#define QUICK_FLASH 1

void flash_write_row(uint32_t *dst, uint32_t *src) {
#if QUICK_FLASH
    bool src_different = false;
    for (int i = 0; i < FLASH_ROW_SIZE / 4; ++i) {
        if (src[i] != dst[i]) {
            src_different = true;
            break;
        }
    }

    if (!src_different) {
        return;
    }
#endif

    flash_erase_row(dst);
    flash_write_words(dst, src, FLASH_ROW_SIZE / 4);
}

void flash_write_nvm_user_config(uint32_t value) {
    // Set automatic page write
    NVMCTRL->CTRLB.bit.MANW = 0;


    // Execute "PBC" Page Buffer Clear
    NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_PBC;
    wait_ready();

    // make sure there are no other memory writes here
    // otherwise we get lock-ups
    *(uint32_t*)NVMCTRL_USER = value;

    // Execute "WAP" Write Page
    NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_WAP;
    wait_ready();
}

void flash_erase_nvm_user_config(void) {
    wait_ready();
    NVMCTRL->STATUS.reg = NVMCTRL_STATUS_MASK;

    // Execute "EAR" Erase Row
    NVMCTRL->ADDR.reg = (uint32_t)NVMCTRL_USER / 2;
    NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_EAR;
    wait_ready();
}

#if APP_START_ADDRESS != 0x2000
#warning "BOOTPROT is not adapted for current bootloader size"
#endif
void flash_bootloader_section_lock(void)
{
  // persistently set bootloader section to 0x2000 to write protect
  uint32_t user_row = *(uint32_t*)NVMCTRL_USER;
  if((user_row & 0x00000007) != BLD_LOCKED)
  {
    user_row = (user_row & 0xfffffff8) | BLD_LOCKED;
    flash_erase_nvm_user_config();
    flash_write_nvm_user_config(user_row);
  }
}

bool flash_bootloader_section_unlock(void)
{
  // unlock bootloader section
  // returns true if bld section was locked before
  uint32_t user_row = *(uint32_t*)NVMCTRL_USER;
  if((user_row & 0x00000007) != BLD_UNLOCKED)
  {
    user_row = user_row | BLD_UNLOCKED;
    flash_erase_nvm_user_config();
    flash_write_nvm_user_config(user_row);
    return true;
  }
  return false;
}