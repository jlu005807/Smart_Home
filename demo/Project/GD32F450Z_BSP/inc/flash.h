#ifndef FLASH_H
#define FLASH_H


#define VOICE_ADDRESS    0x08040000

#define	 PLAYCNT		     80000			


void save_voice(unsigned short *buffer);
void read_voice(unsigned short *buffer);

#endif


