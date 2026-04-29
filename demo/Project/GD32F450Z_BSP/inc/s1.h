#ifndef S1_H
#define S1_H

#include "i2c.h"

#define HT16K33_ADDRESS_S1      0xE8

/* HT16K33  CMD */
#define	SYSTEM_ON				        0x21
#define	SET_INT_NONE		        0xA0
#define	DISPLAY_ON			        0x81
#define	DISPLAY_OFF			        0x80
#define	DIMMING_SET_DEFAULT		  0xEF

#define	KEYKS0				          0x40

#define SWN                     0xFF
#define SW1                     0x01
#define SW2                     0x02
#define SW3                     0x03
#define SW4                     0x04
#define SW5                     0x05
#define SW6                     0x06
#define SW7                     0x07
#define SW8                     0x08
#define SW9                     0x09
#define SWA                     '*'
#define SW0                     0x00
#define SWC                     '#'




i2c_addr_def s1_init(uint8_t address);
uint8_t s1_key_scan(uint32_t i2c_periph,uint8_t i2c_addr);
uint8_t get_key_value(uint32_t i2c_periph,uint8_t i2c_addr);


#endif



