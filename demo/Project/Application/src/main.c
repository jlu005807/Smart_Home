/*************************************************************************************************************
����:     ����ܴ�10�뵹��ʱ��0ʱ,��������
          ���忿��������������ʱ,��������
					���°���1��ʼ¼��,����2����¼��,����3�洢¼��
�汾:     2023-5-17 V1.0
�޸ļ�¼: ��
����:     ZZZ
*************************************************************************************************************/

#include "gd32f4xx.h"
#include "main.h"
#include "i2c.h"
#include "stdio.h"
#include "string.h"
#include "s3.h"
#include "e1.h"
#include "s6.h"
#include "s1.h"
#include "flash.h"
#include "ring_voice.h"



volatile uint16_t delay_count = 0;                 //��ʱ����
volatile uint16_t  time_count = 0;                  //��ʱ����
volatile uint8_t   deal_flag = 0; 


unsigned short playdata[PLAYCNT];
volatile unsigned long playcnt = 0;
volatile unsigned char playflag = 0;
unsigned char sendrecv = 0;

volatile uint8_t  start_flag = 0;
volatile uint8_t  record_flag = 0;
volatile uint8_t  spk_flag = 0;

uint8_t  ms_flag = 0;
uint8_t  ms_count = 0;

i2c_addr_def e1_nixie_tube_addr;
i2c_addr_def s6_addr;
i2c_addr_def s1_key_addr;

uint8_t  key_value;
uint32_t len;

uint8_t  down_count = 10;
uint8_t  dis_data[2];
uint16_t distance = 0;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(void)
{
	  nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);
	  uart_init(USART0);  
	
	  dis_led_init();
	
	  i2c0_gpio_config();
		i2c0_config(); 		
	
		i2c1_gpio_config();
		i2c1_config(); 	
	
	  e1_nixie_tube_addr = e1_init(HT16K33_ADDRESS_E1);
	  
	  s6_addr = s6_init(GD32F330_ADDRESS_S6);
	  
	  s1_key_addr = s1_init(HT16K33_ADDRESS_S1);
	
	  s3_init(); 

    timer3_init();

    e3_sound_recording_config();
		nvic_irq_enable(SPI1_IRQn,0,0); 
		spi_i2s_interrupt_disable(I2S1_ADD, SPI_I2S_INT_RBNE);
		e3_playback_config();
	  
	  if( (playdata[0] == 0xffff) && (playdata[1] == 0xffff) )
	  {
					for(len = 0;len<PLAYCNT;len++)
					{
								playdata[len] = ring_voice[len]; 
					}
					for(len = 0;len<5000;len++)
					{
								playdata[len] = 0;
					} 			
	  }
	
	  while(1)
	  {		
			    if( (deal_flag==1) && (start_flag==1) )
			    {
						    deal_flag = 0;
						    if(s6_read_distance(s6_addr.periph,s6_addr.addr,dis_data,2))
						    {
									   distance = (dis_data[0]<<8) + dis_data[1];
									   if(distance < 300)
										 spk_flag = 0;	 
										 else
                     spk_flag = 1;											 
						    }
						
						    ht16k33_display_float(e1_nixie_tube_addr.periph,e1_nixie_tube_addr.addr,down_count);
						    if(down_count >= 1)
						    down_count -= 1;
								else
								{
									     if(spk_flag)
											 {	 
													 if(playflag == 0)
													 {	 
//														    if(record_flag == 0)
//																{	
//																			for(len = 0;len<PLAYCNT;len++)
//																			{
//																						playdata[len] = ring_voice[len]; 
//																			}
//																			for(len = 0;len<5000;len++)
//																			{
//																						playdata[len] = 0;
//																			} 	  
//															  }
																playcnt = 0;
																playflag = 1;
																LED_ON;
													 }
										   }
                }									
			    }
			    
			    if(ms_flag)	
          {
						    ms_flag = 0;
						    key_value = get_key_value(s1_key_addr.periph,s1_key_addr.addr);
						    switch(key_value)
						    {
											case   SW1:
												     start_flag = 0;	
                             if( ((record_flag == 0) || (record_flag == 2)) && (playflag == 0) )
														 {	 
															    record_flag = 1;
												          playcnt = 0;
														      e3_sound_recording_config();
														      nvic_irq_enable(SPI1_IRQn,0,0);                //ж,¼
															    spi_i2s_interrupt_enable(I2S1_ADD, SPI_I2S_INT_RBNE);
														      LED_OFF;	
														 }					
										  break;
											case   SW2:
												     if(playflag == 0)
														 {	 
															    e3_sound_recording_config();
															    nvic_irq_enable(SPI1_IRQn,0,0); 
															    spi_i2s_interrupt_disable(I2S1_ADD, SPI_I2S_INT_RBNE);
															    e3_playback_config();
															    playcnt = 0;
									                playflag = 1;
									                LED_ON;
														 }
                      break;	
											case   SW3:
                             if(record_flag == 2)
                             {
															     save_voice(playdata);
																 read_voice(playdata);
                                                                 down_count = 10;
                                                                 start_flag = 1;
                             }															 
											break;	
											case   SW4:
												     
											       	 
											break;	
                }
          }
					
			}
}


	
/***********************************************************************************************************
//timer3 init 1ms��ʱ
************************************************************************************************************/
void timer3_init(void)
{
		timer_parameter_struct timer_init_struct;
		
		rcu_periph_clock_enable(RCU_TIMER3);
		
		timer_deinit(TIMER3);
		timer_init_struct.prescaler			= 4199;	
		timer_init_struct.period			= 20;
		timer_init_struct.alignedmode		= TIMER_COUNTER_EDGE;
		timer_init_struct.counterdirection	= TIMER_COUNTER_UP;		
		timer_init_struct.clockdivision		= TIMER_CKDIV_DIV1;		
		timer_init_struct.repetitioncounter = 0;				
		timer_init(TIMER3, &timer_init_struct);
		
		nvic_irq_enable(TIMER3_IRQn, 1, 1); 
		timer_interrupt_enable(TIMER3, TIMER_INT_UP);
		timer_enable(TIMER3);
}



/********************************************************************************************************
//��ʱms����
*********************************************************************************************************/
void delay_ms(uint16_t mstime)
{
     delay_count = mstime;
     while(delay_count)
		 {
		 }	  
}



/********************************************************************************************************
//��ʼ��led
*********************************************************************************************************/
void  dis_led_init (void)
{
    /* enable the led clock */
    rcu_periph_clock_enable(RCU_GPIOD);
    /* configure led GPIO port */ 
    gpio_mode_set(GPIOF, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,GPIO_PIN_15);
    gpio_output_options_set(GPIOF, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,GPIO_PIN_15);

    GPIO_BC(GPIOF) = GPIO_PIN_15;
}



/********************************************************************************************************
//��ʼ������
*********************************************************************************************************/
void uart_init(uint32_t usart_periph)
{
		if(usart_periph == USART0)
		{
				nvic_irq_enable(USART0_IRQn, 0, 0);
				rcu_periph_clock_enable(RCU_GPIOA);	/* enable GPIO clock */
				rcu_periph_clock_enable(RCU_USART0);	/* enable USART clock */
				gpio_af_set(GPIOA, GPIO_AF_7, GPIO_PIN_9);	/* connect port to USARTx_Tx */
				gpio_af_set(GPIOA, GPIO_AF_7, GPIO_PIN_10);	/* connect port to USARTx_Rx */
				gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP,GPIO_PIN_9);	/* configure USART Tx as alternate function push-pull */
				gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,GPIO_PIN_9);
				gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP,GPIO_PIN_10);	/* configure USART Rx as alternate function push-pull */
				gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,GPIO_PIN_10);
				/* USART configure */
				usart_deinit(USART0);
				usart_baudrate_set(USART0,115200U);
				usart_receive_config(USART0, USART_RECEIVE_ENABLE);
				usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
				usart_enable(USART0);
				usart_interrupt_enable(USART0, USART_INT_RBNE);
			

		}
		else if(usart_periph == USART2)
		{
				 nvic_irq_enable(USART2_IRQn, 0, 0);
				 rcu_periph_clock_enable(RCU_GPIOD);	/* enable GPIO clock */
				 rcu_periph_clock_enable(RCU_USART2);	/* enable USART clock */
				 gpio_af_set(GPIOD, GPIO_AF_7, GPIO_PIN_8);	/* connect port to USARTx_Tx */
				 gpio_af_set(GPIOD, GPIO_AF_7, GPIO_PIN_9);	/* connect port to USARTx_Rx */
				 gpio_mode_set(GPIOD, GPIO_MODE_AF, GPIO_PUPD_PULLUP,GPIO_PIN_8);	/* configure USART Tx as alternate function push-pull */
				 gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,GPIO_PIN_8);
				 gpio_mode_set(GPIOD, GPIO_MODE_AF, GPIO_PUPD_PULLUP,GPIO_PIN_9);	/* configure USART Rx as alternate function push-pull */
				 gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,GPIO_PIN_9);
				 /* USART configure */
				 usart_deinit(USART2);
				 usart_baudrate_set(USART2,115200U);
				 usart_receive_config(USART2, USART_RECEIVE_ENABLE);
				 usart_transmit_config(USART2, USART_TRANSMIT_ENABLE);
				 usart_enable(USART2);
				 usart_interrupt_enable(USART2, USART_INT_RBNE);
			

		}
		else if(usart_periph == USART5)
		{
				 nvic_irq_enable(USART5_IRQn, 0, 0);
				 rcu_periph_clock_enable(RCU_GPIOC);	/* enable GPIO clock */
				 rcu_periph_clock_enable(RCU_USART5);	/* enable USART clock */
				 gpio_af_set(GPIOC, GPIO_AF_8, GPIO_PIN_6);	/* connect port to USARTx_Tx */
				 gpio_af_set(GPIOC, GPIO_AF_8, GPIO_PIN_7);	/* connect port to USARTx_Rx */
				 gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_PULLUP,GPIO_PIN_6);	/* configure USART Tx as alternate function push-pull */
				 gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,GPIO_PIN_6);
				 gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_PULLUP,GPIO_PIN_7);	/* configure USART Rx as alternate function push-pull */
				 gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,GPIO_PIN_7);
				 /* USART configure */
				 usart_deinit(USART5);
				 usart_baudrate_set(USART5,115200U);
				 usart_receive_config(USART5, USART_RECEIVE_ENABLE);
				 usart_transmit_config(USART5, USART_TRANSMIT_ENABLE);
				 usart_enable(USART5);
				 usart_interrupt_enable(USART5, USART_INT_RBNE);
			

		}
}




/********************************************************************************************************
//���ڷ�������
*********************************************************************************************************/
uint16_t uart_print(uint32_t usart_periph, uint8_t *data, uint16_t len)
{
	   uint16_t i;
		 for(i = 0; i < len; i++) 
	   {
			   //while(usart_flag_get(usart_periph, USART_FLAG_TC) == RESET);      
			   //usart_data_transmit(usart_periph, '0');
			 
			   //while(usart_flag_get(usart_periph, USART_FLAG_TC) == RESET);      
			   //usart_data_transmit(usart_periph, 'x');
			 
			   while(usart_flag_get(usart_periph, USART_FLAG_TC) == RESET);      
			   usart_data_transmit(usart_periph, data[i]);
			 
			   //while(usart_flag_get(usart_periph, USART_FLAG_TC) == RESET);      
			   //usart_data_transmit(usart_periph, ',');
		 }
		 while(usart_flag_get(usart_periph, USART_FLAG_TC) == RESET);
		 return len;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint16_t uart_print16(uint32_t usart_periph, uint16_t *data, uint16_t len)
{
	   uint16_t i;
	   uint8_t  print_buffer[100];
	
		 for(i = 0; i < len; i++) 
	   {
			     sprintf((char *)print_buffer,(const char*)"0x%x,",data[i]);
			     debug_printf(USART2,(char*)print_buffer);
			     if( ((i % 50) == 0) && (i != 0) )
					 {
						    sprintf((char *)print_buffer,(const char*)"\r\n");
			          debug_printf(USART2,(char*)print_buffer);
           } 						 
		 }
		 while(usart_flag_get(usart_periph, USART_FLAG_TC) == RESET);
		 return len;
}



/********************************************************************************************************
//���Դ��ڷ�������
*********************************************************************************************************/
void debug_printf(uint32_t usart_periph,char *string)
{
     uint8_t  buffer[100];
	   uint16_t len;
	   
	   len = strlen(string);
	   strncpy((char*)buffer,string,len);
	   uart_print(usart_periph,buffer,len);
}
/********************************************************************************************************
//1ms��ʱ�жϷ������
*********************************************************************************************************/
void TIMER3_IRQHandler(void)
{
		if(timer_interrupt_flag_get(TIMER3, TIMER_INT_FLAG_UP))
		{
			   timer_interrupt_flag_clear(TIMER3, TIMER_INT_FLAG_UP);
			
			   if(delay_count > 0)
				 delay_count--;	 
			
			   if(time_count++ >= 1000)
         {
					    time_count = 0;
					    deal_flag = 1;
         }	

         if(ms_count++ >= 20)
				 {
					     ms_count = 0;
					     ms_flag = 1;
				 }	 
		}
}



/********************************************************************************************************
//spi2 i2s�ж�
*********************************************************************************************************/
void SPI1_IRQHandler(void)
{
    if(SET == spi_i2s_interrupt_flag_get(SPI1,SPI_I2S_INT_TBE))
		{
				if(playflag)
				{
						spi_i2s_data_transmit(SPI1, playdata[playcnt++]);
						if(playcnt >= PLAYCNT)
						{
								playflag = 0;	
								playcnt = 0;
							  down_count = 10;
							  LED_OFF;
						}
        }
				else
				{
						spi_i2s_data_transmit(SPI1, 0x0000);
				}
    }

		if(SET == spi_i2s_interrupt_flag_get(I2S1_ADD,SPI_I2S_INT_RBNE))
		{
				playdata[playcnt++] = spi_i2s_data_receive(I2S1_ADD);				
				if(playcnt >= PLAYCNT)
				{
						
						record_flag = 2;		
					  playcnt = 0;
				}
		}
}





