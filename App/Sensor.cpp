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

Sensor::Sensor(Pin key, Pin led, Pin buzzer)
{
	Init();

	Key = new InputPort(key);
	Key->Register(OnPress, this);

	if(led != P0) Led = new OutputPort(led);
	if(buzzer != P0) Buzzer = new OutputPort(buzzer);
}

Sensor::Sensor(Pin key, Pin led, bool ledInvert, Pin buzzer, bool buzzerInvert)
{
	Init();

	Key = new InputPort(key);
	Key->Register(OnPress, this);

	if(led != P0) Led = new OutputPort(led, ledInvert);
	if(buzzer != P0) Buzzer = new OutputPort(key, buzzerInvert);

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

void Sensor::OnPress(InputPort* port, bool down, void* param)
{
	Sensor* btn = (Sensor*)param;
	if(btn) btn->OnPress(port->_Pin, down);
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
