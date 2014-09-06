#ifndef __BUTTON_H__
#define __BUTTON_H__

#include "Sys.h"
#include "Port.h"

// 面板按钮
// 这里必须使用_packed关键字，生成对齐的代码，否则_Value只占一个字节，导致后面的成员进行内存操作时错乱
//__packed class Button
// 干脆把_Value挪到最后解决问题
class Button
{
private:
	InputPort*  _Key;	// 输入按键
	OutputPort* _Led;	// 指示灯
	OutputPort* _Relay;	// 继电器

	static void OnPress(Pin pin, bool down, void* param);
	void OnPress(Pin pin, bool down);

	EventHandler _Handler;
	void* _Param;
public:
	string Name;

	Button(Pin key, Pin led = P0, Pin relay = P0);
	~Button();

	bool GetValue();
	void SetValue(bool value);

	void Register(EventHandler handler, void* param = NULL);

private:

	bool _Value; // 状态
};

#endif
