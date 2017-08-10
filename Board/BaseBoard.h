#ifndef _BaseBoard_H_
#define _BaseBoard_H_

#include "Kernel\Sys.h"

#include "Device\Port.h"

// 板级包基类
class BaseBoard
{
public:
	List<Pin>	LedPins;		// 指示灯 引脚
	List<Pin>	ButtonPins;		// 按键 引脚
	List<OutputPort*>	Leds;	// 指示灯
	List<InputPort*>	Buttons;// 按键
	byte	LedInvert;	// 指示灯倒置。默认自动检测

	BaseBoard();

	// 设置系统参数
	void Init(ushort code, cstring name, int baudRate = 0);
	// 初始化配置区
	void InitConfig();

	void InitLeds();
	void InitButtons(const Delegate2<InputPort&, bool>& press);
};

#endif
