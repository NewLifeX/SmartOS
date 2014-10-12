#include "Button.h"

void Button::Init()
{
	Key = NULL;
	Led = NULL;
	Relay = NULL;

	Name = NULL;
	Index = 0;
	_Value = false;
	_Handler = NULL;
	_Param = NULL;
}

Button::Button(Pin key, Pin led, Pin relay)
{
	Init();

	assert_param(key != P0);
	Key = new InputPort(key);
	Key->Register(OnPress, this);

	if(led != P0) Led = new OutputPort(led);
	if(relay != P0) Relay = new OutputPort(relay);
}

Button::Button(Pin key, Pin led, bool ledInvert, Pin relay, bool relayInvert)
{
	Init();

	assert_param(key != P0);
	Key = new InputPort(key);
	Key->Register(OnPress, this);

	if(led != P0) Led = new OutputPort(led, ledInvert);
	if(relay != P0) Relay = new OutputPort(relay, relayInvert);

	//SetValue(false);
}

Button::~Button()
{
	if(Key) delete Key;
	Key = NULL;

	if(Led) delete Led;
	Led = NULL;

	if(Relay) delete Relay;
	Relay = NULL;
}

void Button::OnPress(Pin pin, bool down, void* param)
{
	Button* btn = (Button*)param;
	if(btn) btn->OnPress(pin, down);
}

void Button::OnPress(Pin pin, bool down)
{
	// 每次按下弹起，都取反状态
	if(!down)
	{
		SetValue(!_Value);

		if(_Handler) _Handler(this, _Param);
	}
}

void Button::Register(EventHandler handler, void* param)
{
	if(handler)
	{
		_Handler = handler;
		_Param = param;
	}
	else
	{
		_Handler = NULL;
		_Param = NULL;
	}
}

bool Button::GetValue() { return _Value; }

void Button::SetValue(bool value)
{
	if(Led) *Led = value;
	if(Relay) *Relay = value;

	_Value = value;
}
