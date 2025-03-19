#include <Wire.h>
#include <Arduino.h>
#include <flash.h>
#include <i2c_flash.h>

#define I2C_SLAVE_ADDRESS 0x30
#define BLINK_HALF_PERIOD_MS 500

#define START_APPLICATION_BYTE_PTR ((volatile uint32_t *)(HMCRAMC0_ADDR + HMCRAMC0_SIZE - 4))
#define START_APPLICATION_MAGIC 0xf02669ef


static I2CFlash g_i2c_flash(&PERIPH_WIRE, PIN_WIRE_SDA, PIN_WIRE_SCL);
static bool is_jump_application_set(void);
static void jump_to_application(void);
static void reboot_to_application(void);

void SERCOM0_Handler()
{
  g_i2c_flash.i2c_flash_it_handler();
}

void setup() {
  if(is_jump_application_set())
  {
    *START_APPLICATION_BYTE_PTR = 0xff;
    jump_to_application();
  }
  pinMode(PIN_LED, OUTPUT);
  g_i2c_flash.begin(I2C_SLAVE_ADDRESS);
}

static void led_toggle()
{
  static bool led_state;
  led_state = !led_state;
  digitalWrite(PIN_LED, led_state);
}

static void blink_loop(){
  static uint32_t last_toggle_time;
  
  if(millis() - last_toggle_time > BLINK_HALF_PERIOD_MS)
  {
    led_toggle();
    last_toggle_time = millis();
  }
}

static bool is_jump_application_set(void)
{
  return (*START_APPLICATION_BYTE_PTR == START_APPLICATION_MAGIC);
}

static void jump_to_application(void)
{
    uint32_t app_reset_handler_address = *(uint32_t *)(FLASH_START_ADDR + 4);
    uint32_t main_stack_pointer_address = *(uint32_t *)FLASH_START_ADDR;
    uint32_t vector_table_address = ((uint32_t)FLASH_START_ADDR & SCB_VTOR_TBLOFF_Msk);

    /* Rebase the Stack Pointer */
    __set_MSP(main_stack_pointer_address);

    /* Rebase the vector table base address */
    SCB->VTOR = vector_table_address;

    /* Jump to application Reset Handler in the application */
    asm("bx %0" ::"r"(app_reset_handler_address));
}

static void reboot_to_application(void)
{
  *START_APPLICATION_BYTE_PTR = START_APPLICATION_MAGIC;
  delay(1);
  NVIC_SystemReset();
}

void loop() {
  if(g_i2c_flash.require_erase)
  {
    flash_erase_to_end((uint32_t*)FLASH_START_ADDR);
    g_i2c_flash.require_erase = false;
  }
  if(g_i2c_flash.flash_write_len)
  {
    g_i2c_flash.write_flash();
    led_toggle(); // blink during flash
  }
  if(g_i2c_flash.require_crc)
  {
    g_i2c_flash.prepare_crc();
  }
  if(g_i2c_flash.require_reboot_to_application)
  {
    reboot_to_application();
  }
  blink_loop();
}
