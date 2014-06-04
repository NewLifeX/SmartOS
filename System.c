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
    COM_NONE,   // MessagePort  消息口
};

void SysInit(void)
{
#ifndef GD32F1
    RCC_ClocksTypeDef clock;
    
    RCC_GetClocksFreq(&clock);
    Sys.Clock = clock.SYSCLK_Frequency;
#endif

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

/*#if DEBUG
// 调试输出
void debug_printf( const char* format, ... )
{
    char    buffer[256];
    va_list arg_ptr;
    byte com;
    int len;

    com = Sys.MessagePort;
    if(com == COM_NONE) return;

    va_start( arg_ptr, format );
    
    len = vsnprintf( buffer, sizeof(buffer)-1, format, arg_ptr );

    // 刷出已存在字符
    Sys.Usart.Flush(com);

    // 写入字符串
    Sys.Usart.Write(com, buffer, len );

    // 刷出新字符
    Sys.Usart.Flush(com);

    va_end( arg_ptr );
}
#endif*/
