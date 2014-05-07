#include "System.h"

#define Execute(module) if(Sys.module.Init) Sys.module.Init(&Sys.module)

void SysInit(void);

TSystem Sys = {SysInit,};

void SysInit(void)
{
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
