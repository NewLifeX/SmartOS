#ifndef __Power_H__
#define __Power_H__

#include "Kernel\Sys.h"

// 电源管理接口
class Power
{
public:
	// 设置电源管理钩子函数
	virtual void SetPower();
	// 电源等级变更（如进入低功耗模式）时调用
	virtual void ChangePower(int level);

	// 低功耗相关
    static void Sleep(int msTime = 0);
	static void Stop(int msTime = 0);
	static void Standby(int msTime = 0);

	// 各模块向系统注册低功耗句柄，供系统进入低功耗前调用
	static void AddPower(Power* power);
	
	// 附加到系统时钟，睡眠时进入低功耗
	static bool AttachTimeSleep();

private:
	static void OnSleep(int msTime);
	static void OnStop(int msTime);
	static void OnStandby(int msTime);
};

#endif
