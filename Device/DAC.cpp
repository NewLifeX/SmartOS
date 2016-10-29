#include "Sys.h"
#include "Device\Port.h"
#include "DAC.h"

//#include "Platform\stm32.h"

#if defined(STM32F1)
DAConverter::DAConverter(Pin pin)
{
	if (pin == P0)
	{
		debug_printf("Pin不能为空");
		return;
	}

	_Pin	= pin;

	OnInit();
}

bool DAConverter::Open()
{
	return OnOpen();
}

bool DAConverter::Close()
{
	return OnClose();
}

bool DAConverter::Write(ushort value)	// 处理对齐问题
{
	return OnWrite(value);
}

#endif
