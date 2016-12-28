#include "WatchDog.h"

WatchDog::WatchDog() { }
WatchDog::~WatchDog()
{
	ConfigMax();
}

WatchDog& WatchDog::Current()
{
    static WatchDog dog;
	return dog;
}

void WatchDog::FeedDogTask(void* param)
{
    WatchDog* dog = (WatchDog*)param;
    dog->Feed();
}

void WatchDog::Start(uint ms, uint msFeed)
{
	static uint		tid = 0;

	auto& dog	= Current();
	if(ms > 20000)
	{
		dog.ConfigMax();

		if(tid) Sys.RemoveTask(tid);
	}
	else
	{
		dog.Config(ms);
		dog.Feed();

		if(!tid && msFeed > 0 && msFeed <= 26000)
		{
			debug_printf("WatchDog::Start ");
			// 首次调度为0ms，让调度系统计算得到其平均耗时，其它任务Sleep时也可以喂狗
			tid = Sys.AddTask(WatchDog::FeedDogTask, &dog, 0, msFeed, "看门狗");
		}
		else
			debug_printf("WatchDog::Config %dms Feed=%dms \r\n", ms, msFeed);
	}
}
