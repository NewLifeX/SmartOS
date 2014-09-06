#include "Button.h"

Button::Button(Pin key, Pin led, Pin relay)
{
	assert_param(key != P0);
	_Key = new InputPort(key);
	_Key->Register(OnPress, this);

	_Led = NULL;
	if(led != P0)
	{
		_Led = new OutputPort(led);
		_Led->Invert = true;
	}

	_Relay = NULL;
	if(relay != P0) _Relay = new OutputPort(relay);

	//_Value = false;
	SetValue(false);

	Name = NULL;
	_Handler = NULL;
	_Param = NULL;
}

Button::~Button()
{
	if(_Key) delete _Key;
	_Key = NULL;

	if(_Led) delete _Led;
	_Led = NULL;

	if(_Relay) delete _Relay;
	_Relay = NULL;
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
	if(_Led) *_Led = value;
	if(_Relay) *_Relay = value;

	_Value = value;
}
