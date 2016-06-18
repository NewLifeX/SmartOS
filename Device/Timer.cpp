#include "Sys.h"

#include "Timer.h"

// 已经实例化的定时器对象
static Timer* Timers[16] = {
	nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr,
};

Timer::Timer(TIMER index)
{
	Timers[index]	= this;

	_index		= index;
	OnInit();

	// 默认情况下，预分频到1MHz，然后1000个周期，即是1ms中断一次
	/*Prescaler = Sys.Clock / 1000000;
	Period = 1000;*/
	SetFrequency(10);

	Opened	= false;

	//_Handler	= nullptr;
	//_Param		= nullptr;
}

Timer::~Timer()
{
	Close();

	//if(_Handler) Register(nullptr);
	if(OnTick.Method) SetHandler(false);

	Timers[_index] = nullptr;
}

// 创建指定索引的定时器，如果已有则直接返回，默认0xFF表示随机分配
Timer* Timer::Create(byte index)
{
	TS("Timer::Create");

	byte tcount	= ArrayLength(Timers);
	// 特殊处理随机分配
	if(index == 0xFF)
	{
		// 找到第一个可用的位置，没有被使用，并且该位置定时器存在
		byte i = 0;
		for(; i<tcount && (Timers[i] || !GetTimer(i)); i++);

		if(i >= tcount)
		{
			debug_printf("Timer::Create 失败！没有空闲定时器！\r\n");
			return nullptr;
		}

		index = i;
	}

	assert(index < tcount, "index");

	if(Timers[index])
		return Timers[index];
	else
		return new Timer((TIMER)index);
}

void Timer::Open()
{
	if(Opened) return;

	TS("Timer::Open");

	OnOpen();

	Opened = true;
}

void Timer::Close()
{
	if(!Opened) return;

	TS("Timer::Close");

	debug_printf("Timer%d::Close\r\n", _index + 1);

	OnClose();

	Opened = false;
}

/*void Timer::Register(EventHandler handler, void* param)
{
	_Handler	= handler;
	_Param		= param;

	SetHandler(handler != nullptr);
}*/

void Timer::Register(const Delegate<Timer&>& dlg)
{
	OnTick	= dlg;

	SetHandler(dlg.Method);
}

void Timer::OnInterrupt()
{
	//if(_Handler) _Handler(this, _Param);
	OnTick(*this);
}

/*================ PWM ================*/

PWM::PWM(TIMER index) : Timer(index)
{
	for(int i=0; i<4; i++) Pulse[i] = 0xFFFF;

	Pulses		= nullptr;
	PulseCount	= 0;
	Channel		= 0;
	PulseIndex	= 0xFF;
	Repeated	= false;
	Configed	= 0x00;
}

void PWM::SetPulse(int idx, ushort pulse)
{
	if(Pulse[idx] == pulse) return;

	Pulse[idx] = pulse;

	if(Opened)
		Config();
	else
		Open();
}
