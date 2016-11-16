#ifndef __Dimmer_H_
#define __Dimmer_H_

#include "Sys.h"
#include "Platform\Pin.h"

class Pwm;
class DimmerConfig;

// 调光
class Dimmer
{
public :
	Pwm*	_Pwm;		// 用于调光的PWM
	bool	Opened;		// 是否已打开
	DimmerConfig*	Config;

	Dimmer();
	~Dimmer();

	// 指定定时器和通道数来初始化
	void Init(TIMER timer, int channels = 0);
	// 指定Pwm和通道数来初始化
	void Init(Pwm* pwm, int channels = 0);

	void Open();
	void Close();

	void Set(byte vs[4]);
	void Change(bool open);

	void Test();

private:
	uint	_task;
	uint	_Pulse[4];
	uint	_Step[4];

	void FlushTask();
};

// 配置
class DimmerConfig : public ConfigBase
{
public:
	byte	Values[4];	// 4通道亮度
	byte	SaveLast;	// 保存最后亮度
	byte	PowerOn;	// 上电是否开灯
	ushort	Gradient;	// 调光时渐变时间，默认2000ms，0表示不使用渐变效果
	byte	TagEnd;

	DimmerConfig();
};

#endif
