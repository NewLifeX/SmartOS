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
	Pwm*	_Pwm;	// 用于调光的PWM
	bool	Opened;	// 是否已打开
	byte	Min;	// 最小值。默认0
	byte	Max;	// 最大值。默认255
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

	void Animate(byte mode, int ms = 3000);

private:
	uint	_task;
	byte	_Pulse[4];
	byte	_Current[4];

	uint	_taskAnimate;
	int		_AnimateData[4];

	void SetPulse(byte vs[4]);
	
	void FlushTask();
	void AnimateTask();
};

// 配置
class DimmerConfig : public ConfigBase
{
public:
	byte	Values[4];	// 4通道亮度
	byte	SaveLast;	// 保存最后亮度
	byte	PowerOn;	// 上电状态。0关1开，0x10动感模式
	byte	Speed;		// 渐变时间，默认10ms，0表示不使用渐变效果
	byte	TagEnd;

	DimmerConfig();
};

#endif
