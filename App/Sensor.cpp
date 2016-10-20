#include "Sensor.h"

void Sensor::Init()
{
	Key = nullptr;
	Led = nullptr;
	Buzzer = nullptr;
	Pir=nullptr;    //门磁
	//Pir=nullptr;    //人体感应
	
	Name = nullptr;
	Index = 0;
	_Value = false;
	//_Handler = nullptr;
	//_Param = nullptr;
}

Sensor::Sensor(Pin key, Pin led, Pin buzzer)
{
	Init();

	Key = new InputPort(key);
	//Key->Register(OnPress, this);
	Key->Press.Bind(&Sensor::OnPress, this);
	Key->UsePress();

	if(led != P0) Led = new OutputPort(led);
	if(buzzer != P0) Buzzer = new OutputPort(buzzer);
}

Sensor::Sensor(Pin key, Pin led, bool ledInvert, Pin buzzer, bool buzzerInvert)
{
	Init();

	Key = new InputPort(key);
	//Key->Register(OnPress, this);
	Key->Press.Bind(&Sensor::OnPress, this);
	Key->UsePress();

	if(led != P0) Led = new OutputPort(led, ledInvert);
	if(buzzer != P0) Buzzer = new OutputPort(key, buzzerInvert);

	//SetValue(false);
}

Sensor::~Sensor()
{
	if(Key) delete Key;
	Key = nullptr;

	if(Led) delete Led;
	Led = nullptr;

	if(Mag) delete Mag;
	Mag = nullptr;
}

/*void Sensor::OnPress(InputPort* port, bool down, void* param)
{
	Sensor* btn = (Sensor*)param;
	if(btn) btn->OnPress(port->_Pin, down);
}*/

void Sensor::OnPress(InputPort& port, bool down)
{
	
	if(!down)
	{
		SetValue(!_Value);

		//if(_Handler) _Handler(this, _Param);
		Press(*this);
	}
}

/*void Sensor::Register(EventHandler handler, void* param)
{
	if(handler)
	{
		_Handler = handler;
		_Param = param;
	}
	else
	{
		_Handler = nullptr;
		_Param = nullptr;
	}
}*/

bool Sensor::GetValue() { return _Value; }

void Sensor::SetValue(bool value)
{
	if(Led) *Led = value;
	if(Buzzer) *Buzzer = value;

	_Value = value;
}
