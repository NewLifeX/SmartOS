#include "Kernel\Sys.h"
#include "Kernel\TTime.h"

#include "Device\Port.h"

#include "ACZero.h"

ACZero::ACZero()
{
	Period = 10000;
	Width = 0;
	Count = 0;
}

ACZero::~ACZero()
{
	if (Port.Opened) Close();
}

void ACZero::Set(Pin pin, byte invert)
{
	if (pin == P0) return;

	Port.HardEvent = true;
	Port.Set(pin);
	if (invert < 2) Port.Invert = invert;
	Port.Press.Bind(&ACZero::OnHandler, this);
	Port.UsePress();
}

bool ACZero::Open()
{
	if (!Port.Open()) return false;

	debug_printf("过零检测引脚探测: ");

	// 等一次零点
	Sys.Sleep(20);
	if (Count > 0)
	{
		debug_printf("已连接交流电！\r\n");
	}
	else
	{
		debug_printf("未连接交流电！\r\n");
		Port.Close();
		return false;
	}

	return true;
}

void ACZero::Close()
{
	Port.Close();
}

static int _Delay = 0;
void ACZero::OnHandler(InputPort& port, bool down)
{
	if (!down)
	{
		Width = port.PressTime;
		return;
	}

	// 每256*10ms修正一次周期
	if (Count++ > 0 && (Count & 0xFF) == 0)
	{
		// 两次零点
		int us = Last.Elapsed();

		// 零点信号可能有毛刺或者干扰，需要避开
		//if (us <= Period / 2 || us >= Period * 2) return;
		if (us <= (Period >> 1) || us >= (Period << 1)) return;

		// 通过加权平均算法纠正数据
		Period = (Period * 31 + us) / 32;

		debug_printf("OnHandler us=%d Period=%d Width=%d _Delay=%d \r\n", us, Period, Width, _Delay);
	}

	Last.Reset();
}

// 等待下一次零点，需要考虑继电器动作延迟
bool ACZero::Wait(int usDelay) const
{
	if (Count == 0) return false;

	// 计算上一次零点后过去的时间
	int us = Last.Elapsed();
	if (us < 0 && us > 40000) return false;

	Sys.Trace(4);

	// 计算下一次零点什么时候到来
	us = Period - us;
	// 继电器动作延迟
	us -= usDelay;

	int d = Period;
	while (us < 0) us += d;
	while (us > d) us -= d;

	//Sys.Trace();
	//debug_printf("ACZero::Wait 周期=%dus 等待=%dus Width=%dms Count=%d Last=%d Now=%d", Period, us, Width, Count, (int)Last, (int)now);
	_Delay = us;

	//TimeCost tc;

	//Sys.Trace();
	//Sys.Delay(us);
	Time.Delay(us);

	//us = tc.Elapsed();
	Sys.Trace(4);
	//debug_printf(" Now2=%d Cost=%d \r\n", (int)Sys.Ms(), us);

	//Sys.Trace();
	return true;
}
