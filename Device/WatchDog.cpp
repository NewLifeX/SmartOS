#include "WatchDog.h"

/*
独立看门狗IWDG——有独立时钟（内部低速时钟LSI---40KHz），所以不受系统硬件影响的系统故障探测器。主要用于监视硬件错误。 
窗口看门狗WWDG——时钟与系统相同。如果系统时钟不走了，这个狗也就失去作用了，主要用于监视软件错误。 

独立看门狗
看门狗定时时限=IWDG_SetReload()的值 / 看门狗时钟频率
看门狗时钟频率=LSI（内部低速时钟）的频率（40KHz）/ 分频数

独立看门狗(IWDG)由专用的40kHz的低速时钟为驱动。因此，即使主时钟发生故障它也仍然有效。
窗口看门狗由从APB1时钟分频后得到的时钟驱动，通过可配置的时间窗口来检测应用程序非正常的过迟或过早的行为。
IWDG_SetPrescaler()		分频，4~256
IWDG_SetReload()		计数值12位 0~0x0FFF
IWDG_ReloadCounter()	喂狗，必须在计数值减到1之前喂狗

IWDG_PR和IWDG_RLR寄存器具有写保护功能。要修改这两个寄存器的值，必须先向IWDG_KR寄存器中写入0x5555。
以不同的值写入这个寄存器将会打乱操作顺序，寄存器将重新被保护。重装载操作(即写入0xAAAA)也会启动写保护功能。

#define IWDG_WriteAccess_Enable     ((uint16_t)0x5555)
#define IWDG_WriteAccess_Disable    ((uint16_t)0x0000)

void IWDG_WriteAccessCmd(uint16_t IWDG_WriteAccess)
{
  assert_param(IS_IWDG_WRITE_ACCESS(IWDG_WriteAccess));
  IWDG->KR = IWDG_WriteAccess;
}

void IWDG_ReloadCounter(void)
{
  IWDG->KR = KR_KEY_Reload;
}
void IWDG_Enable(void)
{
  IWDG->KR = KR_KEY_Enable;
}

void IWDG_SetPrescaler(uint8_t IWDG_Prescaler)
{
  assert_param(IS_IWDG_PRESCALER(IWDG_Prescaler));
  IWDG->PR = IWDG_Prescaler;
}
void IWDG_SetReload(uint16_t Reload)
{
  assert_param(IS_IWDG_RELOAD(Reload));
  IWDG->RLR = Reload;
}

*/

/*WatchDog::WatchDog(uint ms)
{
	Timeout = ms;
	Config(ms);
}*/

WatchDog::~WatchDog()
{
	ConfigMax();
}

void WatchDog::FeedDogTask(void* param)
{
    WatchDog* dog = (WatchDog*)param;
    dog->Feed();
}

void WatchDog::Start(uint ms, uint msFeed)
{
    static WatchDog dog;
	static uint		tid = 0;

	if(ms > 0x0FFF)
		dog.ConfigMax();
	else
		dog.Config(ms);

	if(!tid && msFeed > 0 && msFeed <= 26000)
	{
		debug_printf("WatchDog::Start ");
		tid = Sys.AddTask(WatchDog::FeedDogTask, &dog, msFeed, msFeed, "看门狗");
	}
}
