
#include "PulsePort.h"

PulsePort::PulsePort() { Int(); }

PulsePort::PulsePort(Pin pin)
{
	if (pin == P0)return;
	_Port = new InputPort(pin);
	needFree = true;

	Int();
}
void PulsePort::Int()
{
	LastTriTime = 0;
	TriTime = 0;	//触发时间
	TriCount = 0;	//触发次数
}
bool PulsePort::Set(InputPort * port, uint minIntervals, uint maxIntervals)
{
	if (port == nullptr)return false;

	_Port = port;
	needFree = false;
	MinIntervals = minIntervals;
	MaxIntervals = maxIntervals;
	Int();
	return true;
}

bool PulsePort::Set(Pin pin, uint minIntervals, uint maxIntervals)
{
	if (pin == P0)return false;

	_Port = new InputPort(pin);
	needFree = true;
	MinIntervals = minIntervals;
	MaxIntervals = maxIntervals;
	Int();
	return false;
}

void PulsePort::Open()
{
	if (_Port == nullptr)return;
	if (Handler == nullptr)return;

	if (Opened)return;
	_Port->HardEvent = true;
	if (!_Port->Register([](InputPort* port, bool down, void* param) {((PulsePort*)param)->OnHandler(port, down); }, this))
	{
		debug_printf("PulsePort 注册失败/r/n");
		// 注册失败就返回 不要再往下了  没意义
		return;
	}
	_Port->Open();

	/*_task = Sys.AddTask(
		[](void* param)
	{
		auto port = (PulsePort*)param;
		Sys.SetTask(port->_task, false);
		port->Handler(port, port->Param);
	},
		this, 100, -1, "PulsePort事件");*/

	Opened = true;
}

void PulsePort::Close()
{
	_Port->Close();
	Opened = false;
	if (needFree)
		delete _Port;
}

void PulsePort::InputTask()
{
	if (Handler)
	{
		Handler(this, this->Param);
	}
}
void PulsePort::Register(PulsePortHandler handler, void* param)
{
	if (handler)
	{
		Handler = handler;
		Param = param;
	}
	if (!_task)_task = Sys.AddTask(&PulsePort::InputTask, this, -1, -1, "PulsePort中断");
}

void PulsePort::OnHandler(InputPort* port, bool down)
{
	if (down) return;

	auto preTime = port->PressTime;

	// 默认最大最小为0时候，不做判断
	auto go1 = MinIntervals < preTime&&preTime < MaxIntervals&&MinIntervals != 0 && MaxIntervals != 0;
	auto go2 = preTime < MaxIntervals&&MinIntervals == 0 && MaxIntervals != 0;
	auto go3 = MinIntervals < preTime&&MinIntervals == 0 && MaxIntervals == 0;
	auto go4 = MinIntervals == 0 && MaxIntervals == 0;

	if (!(go1 || go2 || go3 || go4))return;

	// 取UTC时间的MS值
	UInt64 now = Sys.Ms();
	//上次触发时间
	LastTriTime = TriTime;
	//这次触发时间
	TriTime = now;
	debug_printf("PulsePort preTime %ld ", preTime);
	debug_printf("VisTime %ld \r\n", ushort(TriTime - LastTriTime));

	if (_task)Sys.SetTask(_task, true, -1);

	return;
}
