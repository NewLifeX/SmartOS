#include "Kernel\Sys.h"
#include "Device\Port.h"

#include "PulsePort.h"

PulsePort::PulsePort()
{
	Port	= nullptr;
	Min		= 100;
	Max		= 2000;
	Last	= 0;
	Time	= 0;	// 遮挡时间
	Times	= 0;	// 触发次数
}

void PulsePort::Open()
{
	if (Opened) return;

	if (!Port) return;

	Port->HardEvent	= true;
	/*if (!Port->Register([](InputPort* port, bool down, void* param) {((PulsePort*)param)->OnPress(down); }, this))
	{
		debug_printf("PulsePort 注册失败/r/n");
		// 注册失败就返回 不要再往下了  没意义
		return;
	}*/
	Port->Press.Bind(&PulsePort::OnPress, this);
	Port->UsePress();
	Port->Open();

	Opened	= true;
}

void PulsePort::Close()
{
	if (!Opened) return;

	Port->Close();

	Opened	= false;
}

void PulsePort::OnPress(InputPort& port, bool down)
{
	// 只有弹起来才计算
	if (down) return;

	// 计算上一次脉冲以来的遮挡时间，两个有效脉冲的间隔
	UInt64 now	= Sys.Ms();
	if (Last == 0)
	{
		Last = now;
		return;
	}
	auto time	= (uint)(now - Last);
	// 无论如何都更新最后一次时间，避免连续超长
	Last	= now;
	//debug_printf(" timeA %d,  Port  %02X \r\n", time, Port->_Pin);
	// 两次脉冲的间隔必须在一个范围内才算作有效
	if(Min > 0 && time < Min || Max > 0 && time > Max) return;

	Time	= time;
	Times++;
	if (time > 100)
	{
		debug_printf(" time %d,  Port  %02X Last=%d Times=%d\r\n", time, Port->_Pin, (int)Last, Times);
	}
	StartTime = now;

	Press(*this);

	return;
}
