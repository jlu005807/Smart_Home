#include "i2c.h"
#include "s6.h"



/*********************************************************************************************************
函数名:     s6_init
入口参数:   i2c初始地址 address 
出口参数:   无 
返回值:     i2c_addr_def定义结构体
作者:       zzz
日期:       2023/4/6
调用描述:   得到s6 i2c地址,若器件不存在则结构体flag值为0,若存在则初始化芯片置结构体flag值为1
**********************************************************************************************************/
i2c_addr_def s6_init(uint8_t address)
{
     uint8_t i;
	   i2c_addr_def e_address;
	   e_address.flag=0;

     for(i=0;i<4;i++)
     {
			    e_address = get_board_address(address + i*2);
			    if(e_address.flag)
					{
						   break;
          }						
     }
		 
		 return e_address;
}


/*********************************************************************************************************
函数名:     s6_all_init
入口参数:   s6_address结构体指针  s6_addr:s6 i2c起始地址 
出口参数:   s6_address结构体指针
返回值:     无
作者:       zzz
日期:       2023/4/6
调用描述:   查找并初始化所有s6子板,并得到相应的地址放在结构体指针s6_address
**********************************************************************************************************/
void s6_all_init(s6_addr_def *s6_address,uint8_t s6_addr)
{
	   uint8_t i;
	  
	   for(i=0;i<4;i++)
     {
			    s6_address->ult_addr[i] = get_board_address(s6_addr+i*2);
		 }
}



/*********************************************************************************************************
函数名:     s6_read
入口参数:   i2c_periph:I2C口 i2c_addr:i2c地址 read_address:所读寄存器地址 *recvdata:读取数据指针 
            number_of_byte:读取字节数
出口参数:   *recvdata 读到的距离值
返回值:     成功返回1,失败返回0
作者:       zzz
日期:       2023/4/6
调用描述:   读取超声波传感器,并得到距离值
**********************************************************************************************************/
uint8_t s6_read_distance(uint32_t i2c_periph,uint8_t i2c_addr,uint8_t *recvdata,uint16_t number_of_byte)
{
     uint8_t sts = 0;
	
	   sts = i2c_delay_read(i2c_periph,i2c_addr,GD32F330_READ_CMD,recvdata,2);
	
	   return sts;
}	
