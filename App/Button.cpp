#include "Button.h"
#include "ACZero.h"

#include "Kernel\TTime.h"

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
	int us = 0;
	if (Zero)
	{
		// 计算10ms为基数的当前延迟
		int ms = (int)(Sys.Ms() % 10);
		// 而零点以10ms为基数在Zero->Time处，计算需要等待的时间量
		ms = Zero->Time - ms;

		us = ms * 1000;
		// 打开关闭的延迟时间不同，需要减去这个时间量
		us -= value ? DelayOpen : DelayClose;

		while (us < 0) us += 10000;
		while (us > 10000) us -= 10000;

		//Sys.Delay(us);
		Time.Delay(us);
	}
	Led = value;
	Relay = value;

	_Value = value;

	if (Zero)
	{
		debug_printf("Button::SetValue %d 零点=%dus \r\n", value, us);
	}
}
