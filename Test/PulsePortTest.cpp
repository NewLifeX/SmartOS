#include "App\PulsePort.h"
#include "Port.h"

void InitPort();

void TestPulsePort()
{
	InitPort();
}

void PulseHandler(PulsePort* port, bool hasPulse, void* param)
{	
#if defined(DEBUG)
	// down true　无遮挡　　　down false 有遮挡
	static UInt64 HideStr;
	static UInt64 UnHideStr;
	// debug_printf("Press P%c%d down=%d", _PIN_NAME(port->_Port->_Pin),down);
	if(hasPulse)
	{
		// 有脉冲时候统计无脉冲时间
		UnHideStr = Sys.Ms();
		//int time = UnHideStr-HideStr;
		debug_printf("无脉冲时间 %d \r\n", (int)(UnHideStr-HideStr));
	}
	else
	{
		// 无脉冲时候统计有脉冲时间
		HideStr = Sys.Ms();
		//int time = HideStr-UnHideStr;
		debug_printf("有脉冲时间 %d \r\n", (int)(HideStr-UnHideStr));
	}
#endif
}

// 初始化引脚
void InitPort()
{
	static InputPort io(PA6);
	static PulsePort Port;
	Port.Set(&io,35);
	Port.Register(PulseHandler,nullptr);
	Port.Open();
}




