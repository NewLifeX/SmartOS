#ifndef __BUTTON_H__
#define __BUTTON_H__

#include "Sys.h"
#include "Port.h"

// 面板按钮
// 这里必须使用_packed关键字，生成对齐的代码，否则_Value只占一个字节，导致后面的成员进行内存操作时错乱
//__packed class Button
// 干脆把_Value挪到最后解决问题

// 磁保持继电器专用 
// 磁保持继电器特点：  用两根脚控制状态 掉电自动保持状态  
// 且控制只需要一个脉冲  不易长时间上电控制否则影响寿命
class Button_magnetic
{
private:
	void Init();

	static void OnPress(InputPort* port, bool down, void* param);
	void OnPress(Pin pin, bool down);

	EventHandler _Handler;
	void* _Param;
public:
	const char* Name;

	InputPort*  Key;	// 输入按键
	OutputPort* Led;	// 指示灯
	OutputPort* Relay_pack1;	// 继电器引脚1
	OutputPort* Relay_pack2;	// 继电器引脚2

	// 构造函数。指示灯和继电器一般开漏输出，需要倒置
	Button_magnetic() { Init(); }
	Button_magnetic(Pin key = P0, Pin led = P0, bool ledInvert = true, Pin relay_pin1 = P0, 
					bool relayInvert1 = true, Pin relay_pin2 = P0, bool relayInvert2 = true);
	Button_magnetic(Pin key =P0, Pin led = P0, Pin relay_pin1 = P0, Pin relay_pin2 = P0);
	~Button_magnetic();

	bool GetValue();
	void SetValue(bool value);

	void Register(EventHandler handler, void* param = nullptr);

private:
	bool _Value; // 状态
};

#endif
