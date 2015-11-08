#ifndef __Power_H__
#define __Power_H__

#include "Sys.h"

// 电源管理接口
class Power
{
public:
	// 设置电源管理钩子函数
	virtual void SetPower();
	// 电源等级变更（如进入低功耗模式）时调用
	virtual void ChangePower(int level);

	// 低功耗相关
    static void Stop(uint msTime = 0);
	static void DeepSleep(uint msTime = 0);
	static void Standby(uint msTime = 0);

	// 各模块向系统注册低功耗句柄，供系统进入低功耗前调用
	static void AddPower(Power* power);
};

#endif
