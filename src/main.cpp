#include <Wire.h>
#include <Arduino.h>
#include <flash.h>
#include <i2c_flash.h>

#define I2C_SLAVE_ADDRESS 0x30
#define BLINK_HALF_PERIOD_MS 500

static I2CFlash g_i2c_flash(&PERIPH_WIRE, PIN_WIRE_SDA, PIN_WIRE_SCL);

void SERCOM0_Handler()
{
  g_i2c_flash.i2c_flash_it_handler();
}

void setup() {
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

void loop() {
  if(g_i2c_flash.require_erase)
  {
    flash_erase_to_end((uint32_t*)FLASH_START_ADDR);
    g_i2c_flash.require_erase = false;
  }
  if(g_i2c_flash.require_flash_write)
  {
    uint32_t* dst = (uint32_t*)(g_i2c_flash.flash_pointer & 0xfffffffc); // align 4 bytes
    uint32_t* src = (uint32_t*)g_i2c_flash.rx_buffer;
    uint32_t n_words = g_i2c_flash.rx_buffer_index / 4;
    flash_write_words(dst, src, n_words);
    g_i2c_flash.flash_pointer += g_i2c_flash.rx_buffer_index;
    g_i2c_flash.require_flash_write = false;
    led_toggle(); // blink during flash
  }
  blink_loop();
}
