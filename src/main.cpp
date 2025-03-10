#include <Wire.h>
#include <Arduino.h>

#define I2C_SLAVE_ADDRESS 0x50
#define PAGE_SIZE 64
#define FLASH_START_ADDR 0x2100

volatile uint8_t rx_buffer[PAGE_SIZE];
volatile uint32_t rx_index = 0;
volatile uint32_t flash_addr = FLASH_START_ADDR;

void receiveData(int byteCount);
void requestData();
void eraseFlash(uint32_t address);
void writeFlash(uint32_t address, uint8_t *data, uint32_t length);

void setup() {
  pinMode(PIN_LED, OUTPUT);
  // Initialize I2C slave
  Wire.begin(I2C_SLAVE_ADDRESS);
  Wire.onReceive(receiveData);
  Wire.onRequest(requestData);
}

void loop() {
  // Nothing to do in loop, data is handled in interrupt (Wire.onReceive)
  digitalWrite(PIN_LED, 0);
  delay(500);
  digitalWrite(PIN_LED, 1);
  delay(500);
}

void receiveData(int byteCount) {
  static uint32_t write_offset = 0;
  
  if (byteCount == 2) {  
    // Handle EEPROM-style address setting (high byte + low byte)
    uint8_t high = Wire.read();
    uint8_t low = Wire.read();
    write_offset = (high << 8) | low;  // Set write position
  } else {
    // Receive firmware data (EEPROM-style writes)
    while (Wire.available()) {
      uint8_t data = Wire.read();
      rx_buffer[rx_index++] = data;

      if (rx_index >= PAGE_SIZE) {
        writeFlash(FLASH_START_ADDR + write_offset, (uint8_t*)rx_buffer, PAGE_SIZE);
        write_offset += PAGE_SIZE;
        rx_index = 0;
      }
    }
  }
}

void requestData() {
  // Handle any data requests from the I2C master (optional)
  // In this case, no need to send anything back.
}

void eraseFlash(uint32_t address) {
  // Wait for NVMCTRL to be ready and erase a page
  NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_ER;
  while (!NVMCTRL->INTFLAG.bit.READY);
}

void writeFlash(uint32_t address, uint8_t *data, uint32_t length) {
  eraseFlash(address);
  for (uint32_t i = 0; i < length; i += 4) {
    uint32_t word = data[i] | (data[i + 1] << 8) | (data[i + 2] << 16) | (data[i + 3] << 24);
    *((volatile uint32_t*) address) = word;
    address += 4;
    while (!NVMCTRL->INTFLAG.bit.READY);
  }
}