#include "System.h"
#include <stdarg.h>

#define Execute(module) if(Sys.module.Init) Sys.module.Init(&Sys.module)

void SysInit(void);

/* 定义全局Sys对象，指定各成员默认值 */
TSystem Sys = {
    SysInit,
    0,
    
    false,  // Debug        是否调试
    72000000,      // Clock        主频
#if GD32F1
     8000000,      // CystalClock
#endif
    COM1,   // MessagePort  消息口
};

void SysInit(void)
{
#ifndef GD32F1
    RCC_ClocksTypeDef clock;
    
    RCC_GetClocksFreq(&clock);
    Sys.Clock = clock.SYSCLK_Frequency;
#endif
#ifdef STM32F10X
	NVIC_PriorityGroupConfig( NVIC_PriorityGroup_4);	//中断优先级分配方案4   四位都是抢占优先级
#endif
	
	//if(Sys.Core.Init) Sys.Core.Init(&Sys.Core);
	Execute(Core);
	//Execute(Boot);
	Execute(IO);
	Execute(Usart);
	//Execute(Mem);
	//Execute(Flash);
	//Execute(Analog);
	Execute(Spi);
	Execute(nRF);
	/*Execute(Sys.I2c);
	Execute(Sys.Pwm);
	Execute(Sys.Lcd);*/

    Sys.ID[0] = *(__IO uint *)(0X1FFFF7F0); // 高字节
    Sys.ID[1] = *(__IO uint *)(0X1FFFF7EC); // 
    Sys.ID[2] = *(__IO uint *)(0X1FFFF7E8); // 低字节
    Sys.FlashSize = *(__IO ushort *)(0X1FFFF7E0);  // 容量
}
