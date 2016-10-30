#include "Kernel\Sys.h"
#include "Device\Port.h"

#include "PulsePort.h"

PulsePort::PulsePort()
{
	Port = nullptr;
	Min = 0;
	Max = 0;
	Last = 0;
	Time = 0;	// 遮挡时间
	Count = 0;	// 触发次数
	_task = 0;
}

PulsePort::~PulsePort()
{
	Close();
}

static void OnPressTask(void* param)
{
	auto port	= (PulsePort*)param;
	port->Press(*port);
}

void PulsePort::Open()
{
	if (Opened) return;

	if (!Port) return;

	// 如果使用了硬件事件，则这里使用任务来触发外部事件
	if(Port->HardEvent) _task	= Sys.AddTask(OnPressTask, this, -1, -1,"脉冲事件");

	Port->HardEvent = true;
	Port->Press.Bind(&PulsePort::OnPress, this);
	Port->UsePress();
	Port->Open();

	Opened = true;
}

void PulsePort::Close()
{
	if (!Opened) return;

	Sys.RemoveTask(_task);
	Port->Close();

	Opened = false;
}

void PulsePort::OnPress(InputPort& port, bool down)
{
	// 只有弹起来才计算
	if (down) return;

	// 首次触发不算遮挡时间
	UInt64 now = Sys.Ms();
	if (Last == 0)
	{
		Last = now;
		return;
	}
	// 计算上一次脉冲以来的遮挡时间，两个有效脉冲的间隔
	auto st		= Last;
	auto time	= (uint)(now - st);
	// 无论如何都更新最后一次时间，避免连续超长
	Last = now;
	// 两次脉冲的间隔必须在一个范围内才算作有效
	if ((Min > 0 && time < Min) || (Max > 0 && time > Max)) return;

	Start = now;
	Time = time;
	Count++;
	if (time > 100)
	{
		debug_printf(" time %d,  Port  %02X Last=%d Count=%d\r\n", time, Port->_Pin, (int)Last, Count);
	}
	if (_task > 0)
		Sys.SetTask(_task, true, 0);
	else
		Press(*this);
}
