#include "CommandPort.h"
#include "Task.h"

static void CommandTask(void* param);

CommandPort::CommandPort(OutputPort* port, byte* data)
{
	_tid	= 0;

	Port	= port;
	Data	= data;
}

CommandPort::~CommandPort()
{
	Sys.RemoveTask(_tid);
}

void CommandPort::Set(byte cmd)
{
	if(!Port) return;

	OutputPort& port = *Port;
	switch(cmd)
	{
		case 1:
			debug_printf("打开");
			port = true;
			break;
		case 0:
			debug_printf("关闭");
			port = false;
			break;
		case 2:
			debug_printf("反转");
			port = !port;
			break;
		default:
			debug_printf("控制0x%02X", cmd);
			return;
	}
	switch(cmd>>4)
	{
		// 普通指令
		case 0:
			break;
		// 打开，延迟一段时间后关闭
		case 1:
		case 2:
		case 3:
		case 4:
			port = true;
			Next = 0;
			StartAsync(cmd - 0x10);
			break;
		// 关闭，延迟一段时间后打开
		case 5:
		case 6:
		case 7:
		case 8:
			port = false;
			Next = 1;
			StartAsync(cmd - 0x50);
			break;
	}
#if DEBUG
	Name.Show(true);
#endif
}

void CommandPort::Set()
{
	if(!Data) return;

	Set(*Data);

	if(Port) *Data = Port->Read() ? 1 : 0;
}

static void CommandTask(void* param)
{
	CommandPort* port = (CommandPort*)param;
	port->Set(port->Next);
}

void CommandPort::StartAsync(int second)
{
	if(!_tid) _tid = Sys.AddTask(CommandTask, this, -1, -1, "命令端口");
	//Sys.SetTaskPeriod(second * 1000);
	Sys.SetTask(_tid, true, second * 1000);
}
