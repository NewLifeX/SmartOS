
#include "PowerUps.h"
#include "../Config.h"


class PowerUpsCfg : public ConfigBase
{
public:
	byte	Length;			// 数据长度
	byte	ReStartCount;	// 重启次数
	byte	TagEnd;			// 数据区结束标识符

	PowerUpsCfg();

	virtual void Init();
};

PowerUpsCfg::PowerUpsCfg() : ConfigBase()
{
	_Name = "PowUps";
	_Start = &Length;
	_End = &TagEnd;
	Init();
}

void PowerUpsCfg::Init()
{
	Length = Size();
	ReStartCount = 0;
}

/****************************************************************/
/****************************************************************/

PowerUps::PowerUps(byte thld, int timeMs, Func act)
{
	ReStThld = thld;
	if (act)		// 没有动作 直接忽略Flash操作
	{
		PowerUpsCfg pc;
		pc.Load();

		ReStartCount = ++pc.ReStartCount;
		debug_printf("ReStartCount %d\r\n",ReStartCount);

		pc.Save();

		Act = act;
	}
	// 保留他处理一些事情
	Sys.AddTask(DelayAct, this, timeMs, -1, "重启阈值");
}

void PowerUps::DelayAct(void* param)
{
	auto* pu = (PowerUps*) param;
	if (pu->Act)
	{
		if (pu->ReStartCount > pu->ReStThld)pu->Act();
		//pu->ReStartCount = 0;

		PowerUpsCfg pc;
		pc.Load();
		pc.ReStartCount = 0;
		pc.Save();
	}
	// 完成使命 功成身退
	delete pu;
}
