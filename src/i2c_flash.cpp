// inspired by Arduino's TwoWire (Wire.h)

#include "i2c_flash.h"
#include <Arduino.h>
#include <wiring_private.h>
#include "Wire.h"

#define RX_CMD_BUFFER_LEN 2


I2CFlash::I2CFlash(SERCOM *s, uint8_t pinSDA, uint8_t pinSCL) : sercom(s), _uc_pinSDA(pinSDA), _uc_pinSCL(pinSCL) {}

void I2CFlash::begin(uint8_t address, bool enableGeneralCall) {
  //Slave mode
  sercom->initSlaveWIRE(address, enableGeneralCall);
  set_mutli_slave_addr(address);
  sercom->enableWIRE();

  pinPeripheral(_uc_pinSDA, g_APinDescription[_uc_pinSDA].ulPinType);
  pinPeripheral(_uc_pinSCL, g_APinDescription[_uc_pinSCL].ulPinType);
}

void I2CFlash::set_mutli_slave_addr(uint8_t ucAddress)
{
    // TODO: this should not be hardcoded!
        SERCOM0->I2CS.ADDR.reg = SERCOM_I2CS_ADDR_ADDR( ucAddress & 0x7Ful ) | // 0x7F, select only 7 bits
                          SERCOM_I2CS_ADDR_ADDRMASK( 0x03ul );          // 0x00, only match exact address
}


bool I2CFlash::is_ready(void)
{
    bool is_ready =  !(require_erase || require_flash_write);
    return is_ready;
}

void I2CFlash::write_stop(void)
{
    if(rx_cmd_buffer_index == 0) // received 0 bytes
    {
        return;
    }
    else if(rx_cmd_buffer_index == 1) // received 1 byte
    {
        require_erase = true;
    }
    else if(rx_buffer_index > 0) // addr received, >= 1 data bytes received
    {
        require_flash_write = true;
    }
}

bool I2CFlash::receive_byte(uint8_t byte)
{
    if(rx_cmd_buffer_index < RX_CMD_BUFFER_LEN) { // receiving address/command byte
        rx_cmd_buffer[rx_cmd_buffer_index++] = sercom->readDataWIRE();
        if(rx_cmd_buffer_index > 1) // second addr byte received
        {
            // store address if allowed
            size_t addr = rx_cmd_buffer[0] << 8 | rx_cmd_buffer[1];
            if(addr < FLASH_SIZE && addr % 4 == 0) // We only accept 4 byte aligned addresses 
            {
                flash_pointer = addr + (selected_device * FLASH_DEVICE_SIZE) + FLASH_START_ADDR;
                rx_buffer_index = 0;
            }
            else
            {
                return false; 
            }
        }
        return true;
    }
    else if (rx_buffer_index < RX_BUFFER_LEN && (flash_pointer + rx_buffer_index < FLASH_SIZE)) {
        rx_buffer[rx_buffer_index++] = sercom->readDataWIRE();
        return true;
    }
    else{ // too much data
        return false;
    }
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

static volatile uint32_t g_a;

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
            rx_cmd_buffer_index = 0;
        }
    }
    else if(sercom->isAddressMatch())  //Address Match
    {
        selected_device = (uint8_t)((sercom->readDataWIRE() >> 1) & 0x03);
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

