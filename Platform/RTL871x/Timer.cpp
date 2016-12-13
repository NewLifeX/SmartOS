#include "Kernel\Sys.h"
#include "Kernel\Interrupt.h"

#include "Device\Timer.h"

extern "C" {
	#include "RTL871x.h"
	#include "timer_api.h"
}

static uint const g_ts[]	= { TIMER0, TIMER1, TIMER2, TIMER3, TIMER4 };
static gtimer_t* g_Timers[] = { nullptr, nullptr, nullptr, nullptr, nullptr };
const byte Timer::TimerCount = ArrayLength(g_Timers);

const void* Timer::GetTimer(byte idx)
{
	return g_Timers[idx];
}

void Timer::OnInit()
{
	assert(_index <= ArrayLength(g_Timers), "定时器索引越界");

	auto ti	= g_Timers[_index];
	if(!ti)
	{
		ti	= new gtimer_t();
		gtimer_init(ti, g_ts[_index]);
		g_Timers[_index]	= ti;
	}
	_Timer	= ti;
}

void Timer::Config()
{
	TS("Timer::Config");

	// 配置时钟
	auto ti	= (gtimer_t*)_Timer;
	ti->is_periodcal = _TRUE;
    gtimer_reload(ti, Period);
}

void Timer::OnOpen()
{
	debug_printf("Timer%d::Open Period=%d\r\n", _index + 1, Period);

	Config();

    gtimer_start((gtimer_t*)_Timer);
}

void Timer::OnClose()
{
    gtimer_stop((gtimer_t*)_Timer);
}

void Timer::ClockCmd(int idx, bool state)
{
}

// 设置频率，自动计算预分频
void Timer::SetFrequency(uint frequency)
{
	Prescaler	= 1;
	Period		= frequency;

	// 如果已启动定时器，则重新配置一下，让新设置生效
	if(Opened) gtimer_reload((gtimer_t*)_Timer, Period);
}

uint Timer::GetCounter()
{
	return gtimer_read_tick((gtimer_t*)_Timer);
}

void Timer::SetCounter(uint cnt)
{
	// 仅支持0us
	if(!cnt) gtimer_reload((gtimer_t*)_Timer, cnt);
}

static void OnTimerHandler(uint param)
{
	auto timer	= (Timer*)param;
	timer->OnInterrupt();
}

void Timer::SetHandler(bool set)
{
	auto ti	= (gtimer_t*)_Timer;
	if(set)
	{
		ti->handler	= (void*)OnTimerHandler;
		ti->hid		= (uint)this;
	}
	else
	{
		ti->handler	= nullptr;
		ti->hid		= 0;
	}
}

void Timer::OnHandler(ushort num, void* param)
{
	auto timer = (Timer*)param;
	if(timer)
	{
		timer->OnInterrupt();
	}
}
