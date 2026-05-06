#include "gd32f4xx.h"
#include "flash.h"


int8_t flash_opt_write(uint32_t addr,uint32_t len,uint16_t *buff)   
{
			uint32_t i=0;
			uint32_t new_addr;
			uint16_t data=0;
			
			fmc_unlock();
			
			for(i=0;i<len;i++)
			{
				  data = *(buff+i);  
				  new_addr = addr + i*sizeof(uint8_t)*2;
					if(FMC_READY != fmc_halfword_program(new_addr,data))
					{
                        fmc_lock();
						return -1;
					}	
				  fmc_flag_clear(FMC_FLAG_END);
					fmc_flag_clear(FMC_FLAG_WPERR);
					fmc_flag_clear(FMC_FLAG_PGSERR);
					fmc_flag_clear(FMC_FLAG_PGMERR);
			}
			
			fmc_lock();	
			
			return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
//FLASH��ȡ������
uint8_t flash_opt_read(uint32_t addr,uint32_t len,uint16_t *buff)  //������
{                           
			uint32_t i=0;
			uint32_t new_addr;
			for(i=0;i<len;i++)
			{
					new_addr = addr + i*sizeof(uint8_t)*2;
					buff[i] = *(__IO uint16_t *)new_addr;
			}
			
			return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
int8_t flash_opt_m_erase_sector(uint32_t addr)
{   
			fmc_unlock();
	
			fmc_flag_clear(FMC_FLAG_END);
			fmc_flag_clear(FMC_FLAG_WPERR);
			fmc_flag_clear(FMC_FLAG_PGSERR);
			fmc_flag_clear(FMC_FLAG_PGMERR);
			if(FMC_READY != fmc_sector_erase(addr))
			{
				return -1;
			}
			
			fmc_flag_clear(FMC_FLAG_END);
			fmc_flag_clear(FMC_FLAG_WPERR);
			fmc_flag_clear(FMC_FLAG_PGSERR);
			fmc_flag_clear(FMC_FLAG_PGMERR);
			
			fmc_lock();	
			
			return 0;
}


void save_voice(unsigned short *buffer)
{
	    flash_opt_m_erase_sector(CTL_SECTOR_NUMBER_6);
        flash_opt_m_erase_sector(CTL_SECTOR_NUMBER_7);
      	flash_opt_write(VOICE_ADDRESS,PLAYCNT,buffer);
}

void read_voice(unsigned short *buffer)
{
	    flash_opt_read(VOICE_ADDRESS,PLAYCNT,buffer);
}

