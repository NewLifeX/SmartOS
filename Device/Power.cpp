#include "Kernel\TTime.h"

#include "Device\Power.h"
#include "Device\WatchDog.h"

// 低功耗处理器
static List<Power*>	_powers;

void Power::SetPower()
{
	AddPower(this);
}

void Power::ChangePower(int level) { }

void Power::Sleep(int msTime)
{
	//debug_printf("Power::Sleep Time=%d 仅关闭内核，各外设不变，定时唤醒后继续执行 \r\n", msTime);

	OnSleep(msTime);
}

void Power::Stop(int msTime)
{
	debug_printf("Power::Stop Time=%d 关闭内核和外设，靠外部中断唤醒，然后继续执行 \r\n", msTime);

	OnStop(msTime);
}

void Power::Standby(int msTime)
{
	debug_printf("Power::Standby Time=%d 全停，只能等专门的引脚或看门狗来唤醒，重新启动 \r\n", msTime);

	for(int i=0; i<_powers.Count(); i++)
	{
		auto pwr	= _powers[i];
		if(pwr)
		{
			debug_printf("Power::LowPower 0x%p\r\n", pwr);
			pwr->ChangePower(msTime);
		}
	}

	/*if(!msTime) msTime = 0xFFFF;
	WatchDog::Start(msTime);*/

	OnStandby(msTime);
}

// 各模块向系统注册低功耗句柄，供系统进入低功耗前调用
void Power::AddPower(Power* power)
{
	debug_printf("Power::AddPower 0x%p\r\n", power);

	_powers.Add(power);
}

void OnTimeSleep(int ms)
{
	Power::Sleep(ms);
}

// 附加到系统时钟，睡眠时进入低功耗
bool Power::AttachTimeSleep()
{
	((TTime&)Time).OnSleep	= OnTimeSleep;

	return true;
}
