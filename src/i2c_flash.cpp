// inspired by Arduino's TwoWire (Wire.h)

#include "i2c_flash.h"
#include <Arduino.h>
#include <wiring_private.h>
#include "Wire.h"

#define ERASE_COMMAND 0xc7


I2CFlash::I2CFlash(SERCOM *s, uint8_t pinSDA, uint8_t pinSCL) : sercom(s), _uc_pinSDA(pinSDA), _uc_pinSCL(pinSCL) {}

void I2CFlash::begin(uint8_t address, bool enableGeneralCall) {
  //Slave mode
  sercom->initSlaveWIRE(address, enableGeneralCall);
  sercom->enableWIRE();

  pinPeripheral(_uc_pinSDA, g_APinDescription[_uc_pinSDA].ulPinType);
  pinPeripheral(_uc_pinSCL, g_APinDescription[_uc_pinSCL].ulPinType);
}

bool I2CFlash::is_ready(void)
{
    bool is_ready =  !require_erase && flash_write_len == 0;
    return is_ready;
}

void I2CFlash::write_stop(void)
{
    if(rx_buffer_index == 0) // received 0 bytes
    {
        return;
    }
    else if(rx_buffer[0] == ERASE_COMMAND) // erase command
    {
        require_erase = true;
    }
    else if(is_flash_write() && rx_buffer_index > ADDR_LEN) // addr received, >= 1 data bytes received
    {
        // pad buffer
        while(rx_buffer_index % 4 != 0 && rx_buffer_index < RX_BUFFER_LEN)
        {
            rx_buffer[rx_buffer_index++] = 0xff;
        }
        if(flash_pointer >= FLASH_START_ADDR && flash_pointer + rx_buffer_index - ADDR_LEN <= FLASH_START_ADDR + FLASH_SIZE)
        {
            flash_write_len = rx_buffer_index - ADDR_LEN ; // start writing of flash in user space
        }
    }
}

bool I2CFlash::receive_byte(uint8_t byte)
{
    if(rx_buffer_index >= RX_BUFFER_LEN) // rx buffer overflow
    {
        return false;
    }
    
    rx_buffer[rx_buffer_index++] = sercom->readDataWIRE();
    if(rx_buffer_index == ADDR_LEN && is_flash_write())
    {
        // last addr byte received => store address if allowed
        size_t addr = rx_buffer[0] << 24 | rx_buffer[1] << 16 | rx_buffer[2] << 8 | rx_buffer[3];
        if(addr < FLASH_SIZE)
        {
            flash_pointer = addr + FLASH_START_ADDR;
            for(uint8_t i=0; i < flash_pointer % 4; i++) // pad buffer to align by 4 bytes
            {
                rx_buffer[rx_buffer_index++] = 0xff;
            }
        }
        else
        {
            return false;
        }
    }
    return true;
}

void I2CFlash::send_byte(void)
{
    uint8_t c = 0xff;
    if(flash_pointer < FLASH_START_ADDR + FLASH_SIZE)
    {
        c = *(uint8_t*)flash_pointer;
        flash_pointer++;
    }
    sercom->sendDataSlaveWIRE(c);
}

bool I2CFlash::is_flash_write(void)
{
    // first addr bit == 0 => write to flash;
    return rx_buffer_index >= ADDR_LEN && (rx_buffer[0] & 0x80) == 0;
}

// inspired by TwoWire::onService()
void I2CFlash::i2c_flash_it_handler(void)
{
    if(sercom->isStopDetectedWIRE() || 
        (sercom->isAddressMatch() && sercom->isRestartDetectedWIRE() && !sercom->isMasterReadOperationWIRE())) //Stop or Restart detected
    {
        sercom->prepareAckBitWIRE();
        sercom->prepareCommandBitsWire(0x03);
        if(sercom->isMasterReadOperationWIRE())
        {
            flash_pointer--; // for n bytes read, send_byte() is called n+1 times, so we set flash_pointer back
        }
        else
        {
            write_stop();
            rx_buffer_index = 0;
        }
    }
    else if(sercom->isAddressMatch())  //Address Match
    {
        if(is_ready())
        {
            sercom->prepareAckBitWIRE();
        }
        else
        {
            sercom->prepareNackBitWIRE();
        }
        sercom->prepareCommandBitsWire(0x03);
    }
    else if(sercom->isDataReadyWIRE())
    {
      if (sercom->isMasterReadOperationWIRE()) // master is reading
      {
            send_byte();
      } else { //Received data
        uint8_t c = sercom->readDataWIRE();
        if(receive_byte(c))
        {
            sercom->prepareAckBitWIRE();
        }
        else
        {
            sercom->prepareNackBitWIRE();
        }
        sercom->prepareCommandBitsWire(0x03);
      }
    }
}

void I2CFlash::write_flash(void)
{
    uint32_t* dst = (uint32_t*)(flash_pointer & 0xfffffffc); // align 4 bytes
    uint32_t* src = (uint32_t*)(rx_buffer + ADDR_LEN);
    uint32_t n_words = flash_write_len / 4;
    flash_write_words(dst, src, n_words);
    flash_pointer += flash_write_len;
    flash_write_len = 0;
}

