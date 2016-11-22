#include "RasterDrive.h"

RasterOne::RasterOne()
{
}
RasterOne::RasterOne(Pin pin, Pin bz)
{
	Init(pin);
	Buzzer_Init(bz);
}

RasterOne::~RasterOne()
{
	Close();
}

void RasterOne::Buzzer_Init(Pin pin)
{
	Buzzer.Set(pin);
	Buzzer.Invert = 1;
	Buzzer.Blink(2, 100);
	//Buzzer.openDrain=true;
	Buzzer.Open();
	//BnkPort.Interval1	= 50;
	//BnkPort.Interval2	= 100;
	//BnkPort.Add(&Buzzer);
	//BnkPort.Times = 4;
}

void RasterOne::Init(Pin pin)
{
	Port = new PulsePort();
	Port->Port	= new InputPort(pin);
	Min = 100;
	Max = 2000;

}

void RasterOne::SetThld(uint min, uint max)
{
	Min = min;
	Max = max;
}

bool RasterOne::Open()
{
	if (Opened) return true;

	if (Port != nullptr)
	{
		//Port->Register([](PulsePort* port, void* param) {((RasterOne*)(param))->OnHandler(port, param); }, this);
		Port->Press.Bind(&RasterOne::OnHandler, this);
		Port->Open();
		if (Port->Opened)
		{
			debug_printf("Port 初始化成功\r\n");
		}
		else
		{
			debug_printf("Port 初始化失败\r\n");
			delete Port;
			Port = nullptr;
			return false;
		}
	}

	_task = Sys.AddTask(&RasterOne::InputTask, this, -1, -1, "单路光栅事件");

	Opened = true;
	return true;
}

void RasterOne::Close()
{
	if (Port)Port->Close();
	// Sys.SetTask(_task, false);  让任务自己停  避免还遗留数据在此
}
void RasterOne::InputTask()
{
	if (Handler)
	{
		Handler(this, this->Param);
	}
}
void RasterOne::Register(RasterOneHandler handler, void* param)
{
	if (handler)
	{
		Handler = handler;
		Param = param;
	}
}


// 触发信息上报事件
void RasterOne::OnHandler(PulsePort& port)
{

	auto time = port.Time;
	//超过时间
	if (time > Max)
	{
		Last = port.Last;
		TriTime = port.Time;
		return;
	}
	//debug_printf(".");
	if (Min < time)
	{
		TriCount++;
		Last = port.Last;
		TriTime = port.Time;
		WaveLength = ushort(time);
		debug_printf(" RasterOne 波长 %d \r\n", WaveLength);
		/*if (Handler)
		{
			Handler(this, this->Param);
		}*/
		if (_task)Sys.SetTask(_task, true, -1);
		Buzzer.Blink(2, 500);
	}
}

