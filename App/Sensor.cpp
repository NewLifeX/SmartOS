#include "Sensor.h"

void Sensor::Init()
{
	Key = NULL;
	Led = NULL;
	Buzzer = NULL;
	Pir=NULL;    //门磁
  Pir=NULL;    //人体感应
	
	Name = NULL;
	Index = 0;
	_Value = false;
	_Handler = NULL;
	_Param = NULL;
}

Sensor::Sensor(Pin key, Pin led, Pin relay)
{
	Init();

	assert_param(key != P0);
	Key = new InputPort(key);
	Key->Register(OnPress, this);

	if(led != P0) Led = new OutputPort(led);
	if(Buzzer != P0) Buzzer = new OutputPort(Buzzer);
}

Sensor::Sensor(Pin key, Pin led, bool ledInvert, Pin relay, bool relayInvert)
{
	Init();

	assert_param(key != P0);
	Key = new InputPort(key);
	Key->Register(OnPress, this);

	if(led != P0) Led = new OutputPort(led, ledInvert);
	if(Buzzer != P0) key = new OutputPort(key, keyInvert);

	//SetValue(false);
}

Sensor::~Sensor()
{
	if(Key) delete Key;
	Key = NULL;

	if(Led) delete Led;
	Led = NULL;

	if(Mag) delete Mag;
	Mag = NULL;
}

void Sensor::OnPress(Pin pin, bool down, void* param)
{
	Sensor* btn = (Sensor*)param;
	if(btn) btn->OnPress(pin, down);
}

void Sensor::OnPress(Pin pin, bool down)
{
	
	if(!down)
	{
		SetValue(!_Value);

		if(_Handler) _Handler(this, _Param);
	}
}

void Sensor::Register(EventHandler handler, void* param)
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

bool Sensor::GetValue() { return _Value; }

void Sensor::SetValue(bool value)
{
	if(Led) *Led = value;
	if(Buzzer) *Buzzer = value;

	_Value = value;
}
