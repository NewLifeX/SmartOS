#ifndef __BUTTON_H__
#define __BUTTON_H__

#include "Kernel\Sys.h"
#include "Device\Port.h"
#include "Message\DataStore.h"

class ACZero;

// 面板按钮
class Button : public ByteDataPort
{
public:
	cstring	Name;		// 按钮名称
	int		Index;		// 索引号，方便在众多按钮中标识按钮

	InputPort	Key;	// 输入按键
	OutputPort	Led;	// 指示灯
	OutputPort	Relay;	// 继电器

	Delegate<Button&>	Press;	// 按下事件

	const ACZero*	Zero;	// 交流电过零检测
	ushort	DelayOpen;	// 延迟打开继电器us
	ushort	DelayClose;	// 延迟关闭继电器us

	// 构造函数。指示灯和继电器一般开漏输出，需要倒置
	Button();
	virtual ~Button();

	void Set(Pin key, Pin led = P0, bool ledInvert = true, Pin relay = P0, bool relayInvert = true);
	void Set(Pin key, Pin led, Pin relay);
	bool GetValue();
	virtual void SetValue(bool value);

	virtual int OnWrite(byte data);
	virtual byte OnRead();

	String ToString() const;

protected:
	bool _Value; // 状态

	virtual void OnPress(InputPort& port, bool down);
	static void OnKeyPress(Button& btn, InputPort& port, bool down);
};

#endif
