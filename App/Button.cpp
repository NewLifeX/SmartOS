#include "Button.h"
#include "ACZero.h"

Button::Button()
{
	Name = nullptr;
	Index = 0;
	_Value = false;

	Zero = nullptr;
}

Button::~Button()
{
}

void Button::Set(Pin key, Pin led, Pin relay)
{
	Set(key, led, true, relay, true);
}

void Button::Set(Pin key, Pin led, bool ledInvert, Pin relay, bool relayInvert)
{
	Key.Set(key);
	Key.Press.Bind(&Button::OnKeyPress, this);
	Key.UsePress();
	Key.Open();

	if (led != P0)
	{
		Led.Invert = ledInvert;
		Led.Set(led).Open();
	}
	if (relay != P0)
	{
		Relay.Invert = relayInvert;
		Relay.Set(relay).Open();
	}
}

void Button::OnPress(InputPort& port, bool down)
{
	// 每次按下弹起，都取反状态
	if (!down)
	{
		SetValue(!_Value);

		Press(*this);
	}
}

void Button::OnKeyPress(Button& btn, InputPort& port, bool down)
{
	btn.OnPress(port, down);
}

bool Button::GetValue() { return _Value; }

int Button::OnWrite(byte data)
{
	SetValue(data > 0);

	return OnRead();
}

byte Button::OnRead()
{
	return _Value ? 1 : 0;
}

String Button::ToString() const
{
	String str;
	if (Name) str += Name;

	return str;
}

void Button::SetValue(bool value)
{
	if (Zero)
	{
		Sys.Sleep(Zero->Time);

		// 经检测 过零检测电路的信号是  高电平12ms  低电平7ms    即下降沿后8.5ms 是下一个过零点
		// 从给出信号到继电器吸合 测量得到的时间是 6.4ms  继电器抖动 1ms左右  即  平均在7ms上下
		// 故这里添加1ms延时
		// 这里有个不是问题的问题   一旦过零检测电路烧了   开关将不能正常工作
	}
	Led = value;
	Relay = value;

	_Value = value;
}
