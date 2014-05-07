#include "stm32f0xx.h"
#include "System.h"

#ifndef BIT
#define BIT(x)	(1 << (x))
#endif

ushort fac_ms;//全局变量
byte fac_us;//全局变量

/****************************************************
函数功能：延时初始化
输入参数：SYSCLK : 系统时钟(48)MHZ
输出参数：无
备    注：无
*****************************************************/
void delay_init(uint clk)
{
     SysTick->CTRL &=~BIT(2);//选择外部时钟
	 SysTick->CTRL &=~BIT(1);//关闭定时器减到0后的中断请求
	 fac_us = clk/8;//计算好SysTick加载值
	 fac_ms = (ushort)fac_us*1000;	 
}

/****************************************************
函数功能：ms级延时
输入参数：nms : 毫秒
输出参数：无
备    注：调用此函数前，需要初始化Delay_Init()函数
*****************************************************/							    
void delay_ms(uint nms)
{
   	  SysTick->LOAD = (uint)fac_ms*nms-1;//加载时间值
	  SysTick->VAL = 1;//随便写个值，清除加载寄存器的值
	  SysTick->CTRL |= BIT(0);//SysTick使能
	  while(!(SysTick->CTRL&(1<<16)));//判断是否减到0
	  SysTick->CTRL &=~BIT(0);//关闭SysTick
}

/****************************************************
函数功能：us级延时
输入参数：nus : 微秒
输出参数：无
备    注：调用此函数前，需要初始化Delay_Init()函数
*****************************************************/		    								   
void delay_us(uint nus)
{		
	  SysTick->LOAD = (uint)fac_us*nus-1;//加载时间值
	  SysTick->VAL = 1;//随便写个值，清除加载寄存器的值
	  SysTick->CTRL |= BIT(0);//SysTick使能
	  while(!(SysTick->CTRL&(1<<16)));//判断是否减到0
	  SysTick->CTRL &=~BIT(0);//关闭SysTick
}

void TCore_Init(TCore* this)
{
    this->Sleep = delay_ms;
    this->Delay = delay_us;
    
    delay_init(Sys.Clock/1000000);
}
