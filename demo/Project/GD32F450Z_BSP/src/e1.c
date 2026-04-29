#include "i2c.h"
#include <stdio.h>
#include "e1.h"
#include "main.h"
#include "math.h"


//0-F
const uint8_t disdata[17][2] ={{0xF8,0x01},
															 {0x30,0x00},
															 {0xD8,0x02},
															 {0x78,0x02},
															 {0x30,0x03},
															 {0x68,0x03},
															 {0xE8,0x03},
															 {0x38,0x00},
															 {0xF8,0x03},
															 {0x78,0x03},
															 {0xB8,0x03},
															 {0xE0,0x03},
															 {0xC8,0x01},
															 {0xF0,0x02},
															 {0xC8,0x03},
															 {0x88,0x03},
															 {0x00,0x00},
																				 };
															 
const uint8_t point[2] = {0x00,0x04};		
const uint8_t minus[2] = {0x00,0x02};															 
															 
/*********************************************************************************************************
函数名:     get_e1_address
入口参数:   e1相关地址(pca9685或者htk1633地址)
出口参数:   无 
返回值:     e1_address实际得到的结构体地址
作者:       zzz
日期:       2023/3/30
调用描述:   得到pca9685和htk1633的i2c地址,并初始化对应芯片
**********************************************************************************************************/
i2c_addr_def get_e1_address(uint8_t address)
{
     i2c_addr_def e1_addess;

	   e1_addess.flag = 0;
	
	   if(i2c_addr_poll(I2C0,address))
		 {
			    e1_addess.periph = I2C0;
				  e1_addess.addr = address;
					e1_addess.flag = 1;			
		 } 			 
	   if(e1_addess.flag != 1)
		 {
			    if(i2c_addr_poll(I2C1,address))
					{
						  e1_addess.periph = I2C1;
				      e1_addess.addr = address;
					    e1_addess.flag = 1; 
          }						
     }	
     
     if(e1_addess.flag)
		 {
					if( e1_addess.addr <= (PCA9685_ADDRESS_E1+8)  )
					pca9685_init(e1_addess.periph,e1_addess.addr);	
					else if(e1_addess.addr >= HT16K33_ADDRESS_E1)
					ht16k33_init(e1_addess.periph,e1_addess.addr);	
		 } 		 
		 
		 return e1_addess;		 
}  


/*********************************************************************************************************
函数名:     e1_init
入口参数:   e1地址 
出口参数:   无 
返回值:     i2c_addr_def定义结构体
作者:       zzz
日期:       2023/3/28
调用描述:   得到pca9685和htk1633的i2c地址,若器件不存在则结构体flag值为0
**********************************************************************************************************/
i2c_addr_def e1_init(uint8_t address)
{
     i2c_addr_def e1_addess;
	 e1_addess.flag=0;
     uint8_t i;
     
	   for(i=0;i<4;i++)
     {
					if(i2c_addr_poll(I2C0,address+i*2))
					{
							e1_addess.periph = I2C0;
							e1_addess.addr = address+i*2;
							e1_addess.flag = 1;			
							break;
					} 
     }
		 if(e1_addess.flag != 1)
	   {			
					for(i=0;i<4;i++)
					{
							if(i2c_addr_poll(I2C1,address+i*2))
							{
									e1_addess.periph = I2C1;
									e1_addess.addr = address+i*2;
									e1_addess.flag = 1;
									break;
							}	
					}
	   }
		 
		 if(e1_addess.flag)
		 {
			    if( e1_addess.addr <= (PCA9685_ADDRESS_E1+8)  )
					pca9685_init(e1_addess.periph,e1_addess.addr);	
					else if(e1_addess.addr >= HT16K33_ADDRESS_E1)
					ht16k33_init(e1_addess.periph,e1_addess.addr);	
     }	

     return e1_addess;		 
}




/*********************************************************************************************************
函数名:     set_pca9685_pwm_off
入口参数:   i2c_periph I2C口选择 i2c0 or o2c1  i2c_addr 器件地址  
出口参数:   无 
作者:       zzz
日期:       2023/3/28
调用描述:   关闭pca9685所有pwm输出口
**********************************************************************************************************/
void set_pca9685_pwm_off(uint32_t i2c_periph,uint8_t i2c_addr) 
{
    i2c_byte_write(i2c_periph,i2c_addr,ALLLED_ON_L,0);
    i2c_byte_write(i2c_periph,i2c_addr,ALLLED_ON_H,0);
    i2c_byte_write(i2c_periph,i2c_addr,ALLLED_OFF_L,0);
    i2c_byte_write(i2c_periph,i2c_addr,ALLLED_OFF_H,0x10);
}


/*********************************************************************************************************
函数名:     pc9685_init
入口参数:   i2c_periph I2C口选择 i2c0 or o2c1  i2c_addr 器件地址  
出口参数:   无 
作者:       zzz
日期:       2023/3/28
调用描述:   初始化pca9685驱动芯片
***********************************************************************************************************/
void pca9685_init(uint32_t i2c_periph,uint8_t i2c_addr)
{
	  //i2c_byte_write(i2c_periph,i2c_addr,PCA9685_MODE1,0x0);
	  i2c_delay_byte_write(i2c_periph,i2c_addr,PCA9685_MODE1,0x0);
		set_pca9685_pwm_off(i2c_periph,i2c_addr);   
    
}


/*********************************************************************************************************
函数名:     set_pca9685_pwm
入口参数:   i2c_periph I2C口选择 i2c0 or o2c1  i2c_addr 器件地址  
            num PCA9685对应的输出口 on输出使能计数值 off输出关闭计数值（0-15）
出口参数:   无 
作者:       zzz
日期:       2023/3/28
调用描述:   pwm duty = (off - on)/4095   通过on和off的值设置占空比
***********************************************************************************************************/
void set_pca9685_pwm(uint32_t i2c_periph,uint8_t i2c_addr,uint8_t num, uint16_t on, uint16_t off) 
{
    i2c_byte_write(i2c_periph,i2c_addr,LED0_ON_L+4*num,on);
    i2c_byte_write(i2c_periph,i2c_addr,LED0_ON_H+4*num,on>>8);
    i2c_byte_write(i2c_periph,i2c_addr,LED0_OFF_L+4*num,off);
    i2c_byte_write(i2c_periph,i2c_addr,LED0_OFF_H+4*num,off>>8);
}


/*********************************************************************************************************
函数名:    set_pca9685_frequency
入口参数:  i2c_periph I2C口选择 i2c0 or o2c1  i2c_addr 器件地址  frequency 设定的频率值 
出口参数:  无 
作者:      zzz
日期:      2023/3/28
调用描述:  设置PWM频率
prescale value = round(25MHz/(4096*frequency)) - 1 example 30 = (25MHz/(4096*200)) - 1
maximum PWM frequency = 1526 Hz  prescale value = 0x03
minimum PWM frequency = 24 Hz prescale value = 0xff
***********************************************************************************************************/
void set_pca9685_frequency(uint32_t i2c_periph,uint8_t i2c_addr,uint16_t frequency)
{
	   uint8_t prescale_value;
	   uint8_t old_mode,new_mode;
	
     if(frequency > 1526)
		 frequency = 1526;	 
     else if(frequency < 24)
     frequency = 24;
		 
		 i2c_read(i2c_periph,i2c_addr,PCA9685_MODE1,&old_mode,1);
		 new_mode = (old_mode & 0x7F) | 0x10;
     i2c_byte_write(i2c_periph,i2c_addr,PCA9685_MODE1,new_mode);
     prescale_value = (25*1000000/4096/frequency) - 1;
		 i2c_byte_write(i2c_periph,i2c_addr,PCA9685_PRESCALE,prescale_value);
		 i2c_byte_write(i2c_periph,i2c_addr,PCA9685_MODE1,old_mode);
		 delay_ms(20);
		 i2c_byte_write(i2c_periph,i2c_addr,PCA9685_MODE1,old_mode|0xa1);
}


/*********************************************************************************************************
函数名:    e1_rgb_control
入口参数:  i2c_periph I2C口选择 i2c0 or o2c1  i2c_addr 器件地址  red:0-255 green:0-255 blue:0-255  (0-255)对应占空比0-100%  
出口参数:  无 
作者:      zzz
日期:      2023/3/28
描述:      分别填入红绿蓝的占空比,组合颜色和亮度
***********************************************************************************************************/
void e1_rgb_control(uint32_t i2c_periph,uint8_t i2c_addr,uint8_t red,uint8_t green,uint8_t blue)
{	
	  uint16_t off,on;
      
	  on = 0x0f;
	  off = on + red*0x10;    //red*0xfff/0xff
	  set_pca9685_pwm(i2c_periph,i2c_addr,1,on,off);          //red
	   
	  on = 0x0f;
	  off = on + green*0x10;
	  set_pca9685_pwm(i2c_periph,i2c_addr,0,on,off);
	
	  on = 0x0f;
	  off = on + blue*0x10;
	  set_pca9685_pwm(i2c_periph,i2c_addr,2,on,off);
}


/*********************************************************************************************************
函数名:     ht16k33_display_off
入口参数:   i2c_periph I2C口选择 i2c0 or o2c1  i2c_addr 器件地址  
出口参数:   无 
作者:       zzz
日期:       2023/3/28
调用描述:   关闭数码管显示
***********************************************************************************************************/
void ht16k33_display_off(uint32_t i2c_periph,uint8_t i2c_addr)
{
    i2c_byte_write(i2c_periph,i2c_addr,0x02,0x00);
		i2c_byte_write(i2c_periph,i2c_addr,0x03,0x00);
		i2c_byte_write(i2c_periph,i2c_addr,0x04,0x00);
		i2c_byte_write(i2c_periph,i2c_addr,0x05,0x00);
		i2c_byte_write(i2c_periph,i2c_addr,0x06,0x00);
		i2c_byte_write(i2c_periph,i2c_addr,0x07,0x00);
		i2c_byte_write(i2c_periph,i2c_addr,0x08,0x00);
		i2c_byte_write(i2c_periph,i2c_addr,0x09,0x00);
		i2c_cmd_write(i2c_periph,i2c_addr,DISPLAY_ON);
}


/*********************************************************************************************************
函数名:     ht16k33_init
入口参数:   i2c_periph I2C口选择 i2c0 or o2c1  i2c_addr 器件地址  
出口参数:   无 
作者:       zzz
日期:       2023/3/28
调用描述:   初始化数码管芯片驱动ht16k33,并关闭显示
***********************************************************************************************************/
void ht16k33_init(uint32_t i2c_periph,uint8_t i2c_addr)
{
    i2c_cmd_write(i2c_periph,i2c_addr,SYSTEM_ON);
		ht16k33_display_off(i2c_periph,i2c_addr);
}


/*********************************************************************************************************
函数名:     ht16k33_display_data
入口参数:   i2c_periph I2C口选择 i2c0 or o2c1  i2c_addr 器件地址 bit 显示位置  data要显示数据 
出口参数:   无 
作者:       zzz
日期:       2023/3/28
调用描述:   显示4个数码管的数字0-F 0x10则不显示
***********************************************************************************************************/
void ht16k33_display_data(uint32_t i2c_periph,uint8_t i2c_addr,uint8_t bit,uint8_t data)
{
		switch(bit)
		{
				case  1:
					    i2c_byte_write(i2c_periph,i2c_addr,0x02,disdata[data][0]);
					    i2c_byte_write(i2c_periph,i2c_addr,0x03,disdata[data][1]);
				break;
				
				case  2:
					    i2c_byte_write(i2c_periph,i2c_addr,0x04,disdata[data][0]);
					    i2c_byte_write(i2c_periph,i2c_addr,0x05,disdata[data][1]);
				break;
				
				case 3:
					    i2c_byte_write(i2c_periph,i2c_addr,0x06,disdata[data][0]);
					    i2c_byte_write(i2c_periph,i2c_addr,0x07,disdata[data][1]);
				break;
				
				case 4:
					    i2c_byte_write(i2c_periph,i2c_addr,0x08,disdata[data][0]);
					    i2c_byte_write(i2c_periph,i2c_addr,0x09,disdata[data][1]);
				break;
			
		}
		i2c_cmd_write(i2c_periph,i2c_addr,DISPLAY_ON);
}


/*********************************************************************************************************
函数名:     e1_digital_display
入口参数:   i2c_periph I2C口选择 i2c0 or o2c1  i2c_addr 器件地址 
            dis1 dis2 dis3 dis4对应数码管1-4位置的显示数字0-F 
出口参数:   无 
作者:       zzz
日期:       2023/3/28
调用描述:   显示数码管1-4位置相对应的数字(0-F) 0x10则不显示
***********************************************************************************************************/
void e1_digital_display(uint32_t i2c_periph,uint8_t i2c_addr,uint8_t dis1,uint8_t dis2,uint8_t dis3,uint8_t dis4)
{
	   ht16k33_display_data(i2c_periph,i2c_addr,1,dis1);
		 ht16k33_display_data(i2c_periph,i2c_addr,2,dis2);
		 ht16k33_display_data(i2c_periph,i2c_addr,3,dis3);
		 ht16k33_display_data(i2c_periph,i2c_addr,4,dis4);
}


void ht16k33_display_data_point(uint32_t i2c_periph,uint8_t i2c_addr,uint8_t bit,uint8_t data)
{
		switch(bit)
		{
				case  1:
					    i2c_byte_write(i2c_periph,i2c_addr,0x02,disdata[data][0]|point[0]);
					    i2c_byte_write(i2c_periph,i2c_addr,0x03,disdata[data][1]|point[1]);
				break;
				
				case  2:
					    i2c_byte_write(i2c_periph,i2c_addr,0x04,disdata[data][0]|point[0]);
					    i2c_byte_write(i2c_periph,i2c_addr,0x05,disdata[data][1]|point[1]);
				break;
				
				case 3:
					    i2c_byte_write(i2c_periph,i2c_addr,0x06,disdata[data][0]|point[0]);
					    i2c_byte_write(i2c_periph,i2c_addr,0x07,disdata[data][1]|point[1]);
				break;
				
				case 4:
					    i2c_byte_write(i2c_periph,i2c_addr,0x08,disdata[data][0]|point[0]);
					    i2c_byte_write(i2c_periph,i2c_addr,0x09,disdata[data][1]|point[1]);
				break;
			
		}
		i2c_cmd_write(i2c_periph,i2c_addr,DISPLAY_ON);
}


void ht16k33_display_minus(uint32_t i2c_periph,uint8_t i2c_addr,uint8_t bit)
{	
     switch(bit)
		{
				case  1:
					    i2c_byte_write(i2c_periph,i2c_addr,0x02,minus[0]);
					    i2c_byte_write(i2c_periph,i2c_addr,0x03,minus[1]);
				break;
				
				case  2:
					    i2c_byte_write(i2c_periph,i2c_addr,0x04,minus[0]);
					    i2c_byte_write(i2c_periph,i2c_addr,0x05,minus[1]);
				break;
				
				case 3:
					    i2c_byte_write(i2c_periph,i2c_addr,0x06,minus[0]);
					    i2c_byte_write(i2c_periph,i2c_addr,0x07,minus[1]);
				break;
				
				case 4:
					    i2c_byte_write(i2c_periph,i2c_addr,0x08,minus[0]);
					    i2c_byte_write(i2c_periph,i2c_addr,0x09,minus[1]);
				break;
			
		}
		i2c_cmd_write(i2c_periph,i2c_addr,DISPLAY_ON); 
}


void ht16k33_display_float(uint32_t i2c_periph,uint8_t i2c_addr,float number)
{
     float abs_number;
	   float fl_number;
	   uint16_t cal_number;
	   uint8_t  high;
     uint8_t low;
	   uint8_t pt_number;
	
     abs_number = fabs(number); 
	   ht16k33_display_off(i2c_periph,i2c_addr);
     
	   
	   if(abs_number >= 100)
	   {
			    if(abs_number > 999)
			    {
							cal_number = (uint16_t)abs_number;
						  high = cal_number / 100;
							low = cal_number % 100;
						  ht16k33_display_data(i2c_periph,i2c_addr,1,high/10);
						  ht16k33_display_data(i2c_periph,i2c_addr,2,high%10);
							ht16k33_display_data(i2c_periph,i2c_addr,3,low/10); 
							ht16k33_display_data(i2c_periph,i2c_addr,4,low%10); 
			    }
					else
					{	
							if(number < 0)
							ht16k33_display_minus(i2c_periph,i2c_addr,1);
							cal_number = (uint16_t)abs_number;					
							high = cal_number / 100;
							low = cal_number % 100;
							ht16k33_display_data(i2c_periph,i2c_addr,2,high);
							ht16k33_display_data(i2c_periph,i2c_addr,3,low/10); 
							ht16k33_display_data(i2c_periph,i2c_addr,4,low%10); 
				 }
	   } 
		 else
		 {
			    if(abs_number >= 10)
					{
						   cal_number = (uint16_t)abs_number; 
						   fl_number = (abs_number - cal_number)*10;
						   pt_number = (uint8_t)fl_number;
						   if(pt_number > 0)
						   {
						        if(number < 0)
										ht16k33_display_minus(i2c_periph,i2c_addr,1);
                    ht16k33_display_data(i2c_periph,i2c_addr,2,cal_number/10);		
                    ht16k33_display_data_point(i2c_periph,i2c_addr,3,cal_number%10);										
                    ht16k33_display_data(i2c_periph,i2c_addr,4,pt_number);										
						   }
							 else
							 {
								    if(number < 0)
										ht16k33_display_minus(i2c_periph,i2c_addr,2);	
										ht16k33_display_data(i2c_periph,i2c_addr,3,cal_number/10);	
                    ht16k33_display_data(i2c_periph,i2c_addr,4,cal_number%10);										
               }								 
					}	
					else
					{
						   cal_number = (uint16_t)abs_number; 
						   fl_number = (abs_number - cal_number)*10;
						   pt_number = (uint8_t)fl_number;
						   if(pt_number > 0)
						   {
						        if(number < 0)
										ht16k33_display_minus(i2c_periph,i2c_addr,2);
                    ht16k33_display_data_point(i2c_periph,i2c_addr,3,cal_number);										
                    ht16k33_display_data(i2c_periph,i2c_addr,4,pt_number);										
						   }
							 else
							 {
								    if(number < 0)
										ht16k33_display_minus(i2c_periph,i2c_addr,3);	
										ht16k33_display_data(i2c_periph,i2c_addr,4,cal_number);
							 }								 
          }						
     } 			 
}
