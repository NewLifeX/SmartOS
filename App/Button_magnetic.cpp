#include "Button_magnetic.h"

void Button_magnetic::Init()
{
	Key = NULL;
	Led = NULL;
	Relay_pack1 = NULL;
	Relay_pack2 = NULL;
	
	Name = NULL;
	_Value = false;
	_Handler = NULL;
	_Param = NULL;
}

Button_magnetic::Button_magnetic(Pin key, Pin led, Pin relay_pin1, Pin relay_pin2)
{
	Init();

	assert_param(key != P0);
	Key = new InputPort(key);
	Key->Register(OnPress, this);

	if(led != P0) Led = new OutputPort(led);
	if(relay_pin1 != P0) Relay_pack1 = new OutputPort(relay_pin1);
	if(relay_pin2 != P0) Relay_pack2 = new OutputPort(relay_pin2);
	
}

Button_magnetic::Button_magnetic(Pin key, Pin led, bool ledInvert, Pin relay_pin1, bool relayInvert1, Pin relay_pin2, bool relayInvert2)
{
	Init();

	assert_param(key != P0);
	Key = new InputPort(key);
	Key->Register(OnPress, this);

	if(led != P0) Led = new OutputPort(led, ledInvert);
	if(relay_pin1 != P0) Relay_pack1 = new OutputPort(relay_pin1, relayInvert1);
	if(relay_pin2 != P0) Relay_pack2 = new OutputPort(relay_pin2, relayInvert2);
	*Relay_pack1 = false;	// 同初始化为false
	*Relay_pack2 = false;

	SetValue(false);		// 同步继电器状态
}


Button_magnetic::~Button_magnetic()
{
	if(Key) delete Key;
	Key = NULL;

	if(Led) delete Led;
	Led = NULL;

	if(Relay_pack1) delete Relay_pack1;
	Relay_pack1 = NULL;
	
	if(Relay_pack2) delete Relay_pack2;
	Relay_pack2 = NULL;
}


void Button_magnetic::OnPress(Pin pin, bool down, void* param)
{
	Button_magnetic * btn = (Button_magnetic*)param;
	if(btn) btn->OnPress(pin, down);
}

void Button_magnetic::OnPress(Pin pin, bool down)
{
	// 每次按下弹起，都取反状态
	if(!down)
	{
		SetValue(!_Value);

		if(_Handler) _Handler(this, _Param);
	}
}

void Button_magnetic::Register(EventHandler handler, void* param)
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

bool Button_magnetic::GetValue() { return _Value; }


void Button_magnetic::SetValue(bool value)
{
	if(Led) *Led = value;
	if(Relay_pack1 && Relay_pack2)
	{
		if(value)
		{
			*Relay_pack1 = true;
			Sys.Sleep(100);
			*Relay_pack1 = false;
		}
		else
		{
			*Relay_pack2 = true;
			Sys.Sleep(100);
			*Relay_pack2 = false;
		}
	}
	_Value = value;
}
