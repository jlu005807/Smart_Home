#include "i2c.h"
#include "s1.h"


uint8_t bac_key_value = SWN;
uint8_t key_up_cnt = 0;
/*********************************************************************************************************
函数名:     s1_init
入口参数:   i2c初始地址 address 
出口参数:   无 
返回值:     i2c_addr_def定义结构体
作者:       zzz
日期:       2023/4/1
调用描述:   得到s1 i2c地址,若器件不存在则结构体flag值为0,若存在则初始化芯片置结构体flag值为1
**********************************************************************************************************/
i2c_addr_def s1_init(uint8_t address)
{
     i2c_addr_def e_addess;
	 e_addess.flag=0;
     uint8_t i;
	   
	   for(i=0;i<4;i++)
     {	
				 e_addess = get_board_address(address+i*2);
				 
				 if(e_addess.flag)
				 {
						 i2c_cmd_write(e_addess.periph,e_addess.addr,SYSTEM_ON);
					   break;
				 }
	   }
		 
		 return e_addess;		 
}







/*********************************************************************************************
函数名:      s1_key_scan
功能:        按键扫描功能,得到键值
入口参数:    i2c_periph:i2c口  i2c_addr:i2c起始地址
出口参数:    无
返回值：     返回按键扫描键值
作者：       ZZZ
日期:        2023/4/1
调用描述:    调用此函数,得到键值
**********************************************************************************************/
uint8_t s1_key_scan(uint32_t i2c_periph,uint8_t i2c_addr)
{
	  uint8_t key_value;
	  uint8_t keyvalue[6];
	
    i2c_read(i2c_periph,i2c_addr,KEYKS0,keyvalue,6);
	
	   
	  if(keyvalue[0]&0x01)
	  {
		     key_value = SW1;
	  }	
	  else if(keyvalue[2]&0x01)
	  {	
		     key_value = SW2;	 
	  }
	  else if(keyvalue[4]&0x01)
	  {
		     key_value = SW3;
	  }	
	  else if(keyvalue[0]&0x02)
	  {
	       key_value = SW4;
	  }	
	  else if(keyvalue[2]&0x02)
	  {
	       key_value = SW5; 
	  }	
	  else if(keyvalue[4]&0x02)
	  {
	       key_value = SW6; 
	  }
    else if(keyvalue[0]&0x04)
	  {
		     key_value = SW7;
	  }
	  else if(keyvalue[2]&0x04)
	  {
		     key_value = SW8;
	  }
	  else if(keyvalue[4]&0x04)
	  {
		     key_value = SW9;
	  }
	  else if(keyvalue[0]&0x08)
	  {
		     key_value = SWA;
	  }
	  else if(keyvalue[2]&0x08)
	  {
		     key_value = SW0;
	  }
	  else if(keyvalue[4]&0x08)
	  {
		     key_value = SWC;
	  }	
    else 
    {
		     key_value = SWN;
    }	
    
    return key_value;		
}


/*********************************************************************************************
函数名:      get_key_value
功能:        按键扫描功能,得到键值
入口参数:    i2c_periph:i2c口  i2c_addr:i2c起始地址
出口参数:    无
返回值：     返回按键扫描键值
作者：       ZZZ
日期:        2023/4/19
调用描述:    调用此函数,去抖动得到键值
**********************************************************************************************/
uint8_t  get_key_value(uint32_t i2c_periph,uint8_t i2c_addr)
{
	    uint8_t now_key_value = SWN;
	    uint8_t key_value;
	
			key_value = s1_key_scan(i2c_periph,i2c_addr);			
							
			if(key_value != bac_key_value)	
			{
					if(key_value != SWN)
					{
							now_key_value = key_value;
							bac_key_value = key_value;
							key_up_cnt = 0;
					}	
					else
					{
							key_up_cnt += 1;
							if(key_up_cnt >= 1)
							{
									now_key_value = SWN;
									bac_key_value = SWN;
									key_up_cnt = 0;							
							}							
					}					
			}
			else
			{
					if(key_value == SWN)  
					{
							key_up_cnt += 1;
							if(key_up_cnt >= 1)
							{
									now_key_value = SWN;
									bac_key_value = SWN;
									key_up_cnt = 0;		 							
							}							
					}					
			 }

       return now_key_value;
}














