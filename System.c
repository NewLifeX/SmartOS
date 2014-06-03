#include "System.h"

#define Execute(module) if(Sys.module.Init) Sys.module.Init(&Sys.module)

void SysInit(void);

/* 定义全局Sys对象，指定各成员默认值 */
TSystem Sys = {
    SysInit,
    0,
    
    false,  // Debug        是否调试
    0,      // Clock        主频
    0xFF,   // MessagePort  消息口
};

void SysInit(void)
{
    RCC_ClocksTypeDef clock;
    
    RCC_GetClocksFreq(&clock);
    Sys.Clock = clock.SYSCLK_Frequency/1000000;

	//if(Sys.Core.Init) Sys.Core.Init(&Sys.Core);
	Execute(Core);
	Execute(Boot);
	Execute(IO);
	Execute(Usart);
	//Execute(Mem);
	//Execute(Flash);
	//Execute(Analog);
	//Execute(Spi);
	/*Execute(Sys.I2c);
	Execute(Sys.Pwm);
	Execute(Sys.Lcd);*/
}

void TBoot_Init(TBoot* this)
{
}

/*void TLog_Init(TLog* this)
{
	
}*/
