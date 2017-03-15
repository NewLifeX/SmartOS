#include "Button.h"
#include "ACZero.h"

Button::Button()
{
	Name = nullptr;
	Index = 0;
	_Value = false;

	Zero = nullptr;
	DelayOpen = 3800;
	DelayClose = 2600;
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
	// 过零检测
	if (Zero && Zero->Count > 0)
	{
		// 打开关闭的延迟时间不同，需要减去这个时间量
		int us = value ? DelayOpen : DelayClose;
		Zero->Wait(us);
	}

	Led = value;
	Relay = value;

	_Value = value;
}
