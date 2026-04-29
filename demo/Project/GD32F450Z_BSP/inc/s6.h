#ifndef S6_H
#define S6_H

#include "i2c.h"


typedef struct
{
   i2c_addr_def ult_addr[4];
}s6_addr_def;


#define GD32F330_ADDRESS_S6    	  0x58
#define	GD32F330_READ_CMD		      0xAA

i2c_addr_def s6_init(uint8_t address);
void s6_all_init(s6_addr_def *s6_address,uint8_t s6_addr);
uint8_t s6_read_distance(uint32_t i2c_periph,uint8_t i2c_addr,uint8_t *recvdata,  uint16_t number_of_byte);


#endif

