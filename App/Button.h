#ifndef __BUTTON_H__
#define __BUTTON_H__

#include "Sys.h"
#include "Port.h"
#include "Message\DataStore.h"

// 面板按钮
// 这里必须使用_packed关键字，生成对齐的代码，否则_Value只占一个字节，导致后面的成员进行内存操作时错乱
//__packed class Button
// 干脆把_Value挪到最后解决问题
class Button : public Object, public ByteDataPort
{
private:
	//static void OnPress(InputPort* port, bool down, void* param);
	void OnPress(InputPort& port, bool down);

	//EventHandler _Handler;
	//void* _Param;
public:
	cstring	Name;	// 按钮名称
	int		Index;		// 索引号，方便在众多按钮中标识按钮

	InputPort	Key;	// 输入按键
	OutputPort	Led;	// 指示灯
	OutputPort	Relay;	// 继电器

	Delegate<Button&>	Press;	// 按下事件
	
public:
	// 构造函数。指示灯和继电器一般开漏输出，需要倒置
	Button();
	virtual ~Button();

	void Set(Pin key, Pin led = P0, bool ledInvert = true, Pin relay = P0, bool relayInvert = true);
	void Set(Pin key, Pin led, Pin relay);
	bool GetValue();
	void SetValue(bool value);

	//void Register(EventHandler handler, void* param = nullptr);

	virtual int OnWrite(byte data);
	virtual byte OnRead();

	virtual String& ToStr(String& str) const;

// 过零检测
private:
	static int ACZeroAdjTime;			// 过零检测时间补偿  默认 2300us

public:
	static InputPort	ACZero;			// 交流过零检测引脚

	static bool SetACZeroPin(Pin aczero);	// 设置过零检测引脚
	static void SetACZeroAdjTime(int us){ ACZeroAdjTime = us; };	// 设置 过零检测补偿时间

private:
	bool _Value; // 状态
};

#endif
