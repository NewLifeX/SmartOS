#include "App\PulsePort.h"
#include "Device\Port.h"

void InitPort();

void TestPulsePort()
{
	InitPort();
}

static void PulseHandler(PulsePort& port)
{	
#if defined(DEBUG)
	// down true　无遮挡　　　down false 有遮挡
	//static UInt64 HideStr;
	//static UInt64 UnHideStr;
	// debug_printf("Press P%c%d down=%d", _PIN_NAME(port->_Port->_Pin),down);	
#endif
}

// 初始化引脚
void InitPort()
{
	static InputPort io(PA6);
	static PulsePort Port;
	//Port.Set(&io,35);
	//Port.Register(PulseHandler, nullptr);
	Port.Port	= &io;
	Port.Min	= 35;
	Port.Press	= PulseHandler;
	Port.Open();
}




