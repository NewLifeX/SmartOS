#ifndef __BUTTON_GRAY_LEVEL_H__
#define __BUTTON_GRAY_LEVEL_H__

#include "Config.h"
#include "Device\Pwm.h"

#include "Button.h"

struct ButtonPin
{
	Pin Led;
	Pin Key;
	Pin Relay;
	bool Invert;
	byte PwmIndex;
};
// 配置
class Button_GrayLevelConfig : public ConfigBase
{
public:
	byte	OnGrayLevel; // 开灯灰度
	byte	OffGrayLevel;// 关灯灰度
	byte	TagEnd;
	Button_GrayLevelConfig();
};

class Button_GrayLevel;
using TAction = Delegate<Button&>::TAction;

// 面板按钮
// 这里必须使用_packed关键字，生成对齐的代码，否则_Value只占一个字节，导致后面的成员进行内存操作时错乱
//__packed class Button
// 干脆把_Value挪到最后解决问题
extern Button_GrayLevelConfig*	ButtonConfig;

class Button_GrayLevel : public Button
{
public:
	bool EnableDelayClose;			// 标识是否启用延时关闭功能,默认启用

	// 构造函数。指示灯和继电器一般开漏输出，需要倒置
	Button_GrayLevel();

	static Button_GrayLevelConfig InitGrayConfig();

	using Button::Set;

	// led 驱动器设置
	void Set(Pwm* drive, byte pulseIndex);

	virtual void SetValue(bool value);
	void RenewGrayLevel();

public:
	static void Init(TIMER tim, byte count, Button_GrayLevel* btns, TAction onpress, const ButtonPin* pins, byte* level, const byte* state);
	static void UpdateLevel(byte* level, Button_GrayLevel* btns, byte count);

private:
	// 指示灯灰度驱动器 Pwm;
	Pwm* 	_Pwm;
	byte	_Channel;

	bool _Value; // 状态
	ushort Reserved;	// 补足对齐问题

	virtual void OnPress(InputPort& port, bool down);

	void GrayLevelDown();
	void GrayLevelUp();
	void DelayClose2(int ms);		// 自定义延时关闭
	int delaytime = 0;
	uint _task2 = 0;
	void OnDelayClose();

	//上报状态变化
	uint _task3 = 0;

	void	ReportPress();
};

#endif
