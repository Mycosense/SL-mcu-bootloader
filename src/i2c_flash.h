#ifndef i2c_flash_h
#define i2c_flash_h

#include "Stream.h"
#include "variant.h"

#include "Wire.h"
#include "SERCOM.h"
#include "flash.h"


#define RX_BUFFER_LEN FLASH_ROW_SIZE
#define FLASH_START_ADDR 0x2000U

class I2CFlash
{
  public:
    I2CFlash(SERCOM *s);
    void i2c_flash_it_handler(void);
    
    volatile uint8_t rx_buffer[RX_BUFFER_LEN];
    volatile uint16_t rx_buffer_index;
    volatile bool require_erase;
    volatile bool require_flash_write;
    volatile size_t flash_pointer = FLASH_START_ADDR;

    
  private:
    bool is_ready(void);
    void write_stop(void);
    bool receive_byte(uint8_t byte);
    void send_byte(void);

    volatile uint8_t rx_cmd_buffer[2];
    volatile uint8_t rx_cmd_buffer_index;

    SERCOM* sercom;
};
#endif // i2c_flash_h