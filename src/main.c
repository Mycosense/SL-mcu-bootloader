/* ----------------------------------------------------------------------------
 *         SAM Software Package License
 * ----------------------------------------------------------------------------
 * Copyright (c) 2011-2014, Atmel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following condition is met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ----------------------------------------------------------------------------
 */

/**
 * --------------------
 * SAM-BA Implementation on SAMD21
 * --------------------
 * Requirements to use SAM-BA :
 *
 * Supported communication interfaces :
 * --------------------
 *
 * SERCOM5 : RX:PB23 TX:PB22
 * Baudrate : 115200 8N1
 *
 * USB : D-:PA24 D+:PA25
 *
 * Pins Usage
 * --------------------
 * The following pins are used by the program :
 * PA25 : input/output
 * PA24 : input/output
 * PB23 : input
 * PB22 : output
 * PA15 : input
 *
 * The application board shall avoid driving the PA25,PA24,PB23,PB22 and PA15
 * signals
 * while the boot program is running (after a POR for example)
 *
 * Clock system
 * --------------------
 * CPU clock source (GCLK_GEN_0) - 8MHz internal oscillator (OSC8M)
 * SERCOM5 core GCLK source (GCLK_ID_SERCOM5_CORE) - GCLK_GEN_0 (i.e., OSC8M)
 * GCLK Generator 1 source (GCLK_GEN_1) - 48MHz DFLL in Clock Recovery mode
 * (DFLL48M)
 * USB GCLK source (GCLK_ID_USB) - GCLK_GEN_1 (i.e., DFLL in CRM mode)
 *
 * Memory Mapping
 * --------------------
 * SAM-BA code will be located at 0x0 and executed before any applicative code.
 *
 * Applications compiled to be executed along with the bootloader will start at
 * 0x2000 (samd21) or 0x4000 (samd51)
 * The bootloader doesn't changes the VTOR register, application code is 
 * taking care of this.
 *
 */

#include "uf2.h"
#include "crc16_ccitt.h"

#define BOOTLOADER_TIMEOUT 100000 // approx 5s
#define BLD_METABLOCK_SIZE 0x100
static const uint8_t BLD_METABLOCK_MAGIC[] = {0x01, 0x05};

static bool verify_application_code(void);
static bool is_metablock(void);

static volatile bool main_b_cdc_enable = false;
extern int8_t led_tick_step;

#if defined(SAMD21)
    #define RESET_CONTROLLER PM
#elif defined(SAMD51)
    #define RESET_CONTROLLER RSTC
#endif

/**
 * \brief Check the application startup condition
 *
 */
static void check_start_application(void) {

#if USE_SINGLE_RESET
    if (SINGLE_RESET()) {
        if (RESET_CONTROLLER->RCAUSE.bit.POR || *DBL_TAP_PTR != DBL_TAP_MAGIC_QUICK_BOOT) {
            // the second tap on reset will go into app
            *DBL_TAP_PTR = DBL_TAP_MAGIC_QUICK_BOOT;
            // this will be cleared after succesful USB enumeration
            // this is around 1.5s
            resetHorizon = timerHigh + 50;
            return;
        }
    }
#endif

    if (RESET_CONTROLLER->RCAUSE.bit.POR) {
        *DBL_TAP_PTR = 0;
    }
    else if (*DBL_TAP_PTR == DBL_TAP_MAGIC) {
        *DBL_TAP_PTR = 0;
        return; // stay in bootloader
    }
    else {
        if (*DBL_TAP_PTR != DBL_TAP_MAGIC_QUICK_BOOT) {
            *DBL_TAP_PTR = DBL_TAP_MAGIC;
            delay(500);
        }
        *DBL_TAP_PTR = 0;
    }



    LED_MSC_OFF();
    RGBLED_set_color(COLOR_LEAVE);

    uint32_t app_start_address;
    if(is_metablock())
    {
        if(!verify_application_code())
        {
            return;  // invalid meta data, stay in bootloader
        }
        app_start_address = APP_START_ADDRESS + BLD_METABLOCK_SIZE;
    }
    else
    {
        // Legacy firmware w/o metablock
        app_start_address = APP_START_ADDRESS;
    }

    uint32_t app_reset_handler_address = *(uint32_t *)(app_start_address + 4);
    uint32_t main_stack_pointer_address = *(uint32_t *)app_start_address;
    uint32_t vector_table_address = ((uint32_t)app_start_address & SCB_VTOR_TBLOFF_Msk);

    // Reset Handler sanity check
    if (app_reset_handler_address < app_start_address || app_reset_handler_address > FLASH_SIZE) {
        /* Stay in bootloader */
        return;
    }

    /* Rebase the Stack Pointer */
    __set_MSP(main_stack_pointer_address);

    /* Rebase the vector table base address */
    SCB->VTOR = vector_table_address;

    /* Jump to application Reset Handler in the application */
    asm("bx %0" ::"r"(app_reset_handler_address));
}

extern char _etext;
extern char _end;

/**
 *  \brief SAMD21 SAM-BA Main loop.
 *  \return Unused (ANSI-C compatibility).
 */
int main(void) {
    // if VTOR is set, we're not running in bootloader mode; halt
    if (SCB->VTOR)
        while (1) {
        }

    // Disable the watchdog, in case the application set it.
#if defined(SAMD21)
    WDT->CTRL.reg = 0;
    while(WDT->STATUS.bit.SYNCBUSY) {}
#elif defined(SAMD51)
    WDT->CTRLA.reg = 0;
    while(WDT->SYNCBUSY.reg) {}
#endif

#if USB_VID == 0x239a && USB_PID == 0x0013     // Adafruit Metro M0
    // Delay a bit so SWD programmer can have time to attach.
    delay(15);
#endif
    led_init();

    logmsg("Start");
    assert((uint32_t)&_etext < APP_START_ADDRESS);
    // bossac writes at 0x20005000
    assert(!USE_MONITOR || (uint32_t)&_end < 0x20005000);

    assert(8 << NVMCTRL->PARAM.bit.PSZ == FLASH_PAGE_SIZE);
    assert(FLASH_PAGE_SIZE * NVMCTRL->PARAM.bit.NVMP == FLASH_SIZE);

    /* Jump in application if condition is satisfied */
    check_start_application();

    /* We have determined we should stay in the monitor. */
    /* System initialization */
    system_init();

    __DMB();
    __enable_irq();

#if USE_UART
    /* UART is enabled in all cases */
    usart_open();
#endif

    logmsg("Before main loop");

    usb_init();

    // not enumerated yet
    RGBLED_set_color(COLOR_START);
    led_tick_step = 10;


#ifdef JETSON_ENABLE_PIN
    PINOP(JETSON_ENABLE_PIN, DIRSET);
    PINOP(JETSON_ENABLE_PIN, OUTSET);
#endif

    /* Wait for a complete enum on usb or a '#' char on serial line until timeout */
    for(uint32_t i = 0; i < BOOTLOADER_TIMEOUT; i++) {
        if (USB_Ok()) {
            if (!main_b_cdc_enable) {
#if USE_SINGLE_RESET
                // this might have been set
                resetHorizon = 0;
#endif
                RGBLED_set_color(COLOR_USB);
                led_tick_step = 1;

#if USE_SCREEN
                screen_init();
                draw_drag();
#endif
            }

            main_b_cdc_enable = true;
        }

#if USE_MONITOR
        // Check if a USB enumeration has succeeded
        // And com port was opened
        if (main_b_cdc_enable) {
            logmsg("entering monitor loop");
            // SAM-BA on USB loop
            while (1) {
                sam_ba_monitor_run();
            }
        }
#if USE_UART
        /* Check if a '#' has been received */
        if (!main_b_cdc_enable && usart_sharp_received()) {
            RGBLED_set_color(COLOR_UART);
            sam_ba_monitor_init(SAM_BA_INTERFACE_USART);
            /* SAM-BA on UART loop */
            while (1) {
                sam_ba_monitor_run();
            }
        }
#endif
#else // no monitor
        if (main_b_cdc_enable) {
            process_msc();
        }
#endif

        if (!main_b_cdc_enable) {
            // get more predictable timings before the USB is enumerated
            for (int i = 1; i < 256; ++i) {
                asm("nop");
            }
        }
    }
    // after timeout, we restart
    NVIC_SystemReset();
}

// True: success (application code valid)
static bool verify_application_code(void)
{
    uint16_t crc = *(uint16_t*)(APP_START_ADDRESS + 2);
    uint32_t firmware_size = *(uint32_t*)(APP_START_ADDRESS + 4);
    uint16_t crc_calculated = crc16_calc((uint8_t*)(APP_START_ADDRESS + BLD_METABLOCK_SIZE), firmware_size, 0);
    if(crc == crc_calculated)
    {
        return true;
    }
    else
    {
        return false;
    }
}

static bool is_metablock(void)
{
    return memcmp(BLD_METABLOCK_MAGIC, (uint8_t*)APP_START_ADDRESS, sizeof(BLD_METABLOCK_MAGIC)) == 0;
}