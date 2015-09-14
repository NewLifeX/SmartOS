#ifndef __BUTTON_GRAY_LEVEL_H__
#define __BUTTON_GRAY_LEVEL_H__

#include "Sys.h"
#include "Port.h"
#include "Timer.h"
// 面板按钮
// 这里必须使用_packed关键字，生成对齐的代码，否则_Value只占一个字节，导致后面的成员进行内存操作时错乱
//__packed class Button
// 干脆把_Value挪到最后解决问题
class Button_GrayLevel
{
private:
	static void OnPress(Pin pin, bool down, void* param);
	void OnPress(Pin pin, bool down);

	EventHandler _Handler;
	void* _Param;
	// 指示灯灰度驱动器 PWM;
	PWM* 	_GrayLevelDrive;
	byte	_PulseIndex;
	
	
public:
	string	Name;		// 按钮名称
	int		Index;		// 索引号，方便在众多按钮中标识按钮

	InputPort	Key;	// 输入按键
	OutputPort	Relay;	// 继电器

	static byte OnGrayLevel;	// 开灯时 led 灰度
	static byte OffGrayLevel;	// 关灯时 led 灰度
public:
	// 构造函数。指示灯和继电器一般开漏输出，需要倒置
	Button_GrayLevel();
	~Button_GrayLevel();
	
	void Set(Pin key, Pin relay = P0, bool relayInvert = true);
	void Set(Pin key, Pin relay);
	// led 驱动器设置
	void Set(PWM* drive, byte pulseIndex);
	bool GetValue();
	void SetValue(bool value);
	void RenewGrayLevel();
	void Register(EventHandler handler, void* param = NULL);

// 过零检测
private:
	static int ACZeroAdjTime;			// 过零检测时间补偿  默认 2300

public:
	static InputPort*  ACZero;			// 交流过零检测引脚

	static bool SetACZeroPin(Pin aczero);	// 设置过零检测引脚
	static void SetACZeroAdjTime(int us){ ACZeroAdjTime = us; };	// 设置 过零检测补偿时间

private:
	bool _Value; // 状态
};

#endif
