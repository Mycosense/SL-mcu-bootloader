#ifndef _CRC16_H_
#define _CRC16_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus                                                                                                       
extern "C" {                                                                                                             
#endif


uint16_t crc16_ccitt(const uint8_t* buffer, size_t size);


#ifdef __cplusplus                                                                                                       
}
#endif

#endif // _CRC16_H_