#include "Device\Port.h"
#include "App\PulsePort.h"

#include "Raster.h"


static Stream*	_Cache = nullptr;		//实际送的数据
static uint		_RasterTask = 0;
static int		_Ras = 0;

static PulsePort* Create(Pin pin, uint min, uint max, bool filter)
{
	auto pp = new PulsePort();
	pp->Port = new InputPort(pin);
	pp->Port->HardEvent = true;
	pp->Min = min;
	pp->Max = max;
	pp->Filter = filter;

	return pp;
}

Raster::Raster(Pin pinA, Pin pinB)
{
	Init();

	RasterA = Create(pinA, Min, Max, Filter);

	RasterB = Create(pinB, Min, Max, Filter);

	_Ras++;
}

Raster::~Raster()
{
	delete RasterA->Port;
	delete RasterA;
	delete RasterB->Port;
	delete RasterB;

	if (--_Ras == 0)
	{
		delete _Cache;
		Sys.RemoveTask(_task);
	}
}

void Raster::Init()
{
	RasterA = nullptr;
	RasterB = nullptr;
	Opened = false;

	FlagB.Start = 0;
	FlagB.Time = 0;
	FlagB.Count = 0;

	FlagA.Start = 0;
	FlagA.Time = 0;
	FlagA.Count = 0;

	Min = 100;		// 最小时间间隔 单位 ms
	Max = 2000;		// 最大时间间隔 单位 ms

	Filter = false;

	Count = 0;
}

bool Raster::Open()
{
	if (Opened) return true;

	if (RasterA != nullptr)
	{
		RasterA->Press.Bind(&Raster::OnHandlerA, this);
		RasterA->Open();
	}

	if (RasterB != nullptr)
	{
		RasterB->Press.Bind(&Raster::OnHandlerB, this);
		RasterB->Open();
	}

	if (!_Cache)
	{
		_Cache = new MemoryStream();
		_Cache->SetCapacity(512);
	}
	// 光栅一直在轮训是否有数据要发送
	if (!_RasterTask) _RasterTask = Sys.AddTask(&Raster::Report, this, 3000, 2500, "光栅发送");

	Opened = true;

	return true;
}

// 检查配对
static bool CheckMatch(FlagData& flag, UInt64 now)
{
	if (flag.Count == 0)
	{
		debug_printf("等待配对\r\n");
		return false;
	}

	// 超过3秒无效
	if (flag.Start + 3000 < now)
	{
		debug_printf("过期清零\r\n");
		flag.Count = 0;
		return false;
	}

	return true;
}

void Raster::OnHandlerA(PulsePort& raster)
{
	//Buzzer.Blink(2, 500);
	debug_printf("A路触发，波长%d \r\n", raster.Time);
	FlagA.Start = raster.Start;
	FlagA.Time = raster.Time;
	FlagA.Count++;

	if (CheckMatch(FlagB,FlagA.Start))
	{
		LineReport();
		debug_printf("A配对\r\n");
	}

}

void Raster::OnHandlerB(PulsePort& raster)
{
	//Buzzer.Blink(2, 500);
	debug_printf("B路触发，波长%d \r\n", raster.Time);
	FlagB.Start = raster.Start;
	FlagB.Time = raster.Time;
	debug_printf("获得波长%d\r\n", FlagB.Time);
	FlagB.Count++;

	if (CheckMatch(FlagA, FlagB.Start))
	{
		LineReport();
		debug_printf("B配对\r\n");
	}

}

void Raster::LineReport()
{
	auto size = sizeof(RasTriData);
	Count++;
	Stop = true;
	// 构造当前数据
	RasTriData data;
	data.Index = Index;
	data.Start = DateTime::Now().TotalMs();
	data.TimeA = FlagA.Time;
	data.TimeB = FlagB.Time;
	debug_printf("data获得波长%d\r\n", data.TimeB);
	data.Count = Count;

	// 时间差加方向
	if (FlagA.Start > FlagB.Start)
	{
		data.Direction = 0x00;
		data.Time = (ushort)(FlagA.Start - FlagB.Start);
	}
	else
	{
		data.Direction = 0x01;
		data.Time = (ushort)(FlagB.Start - FlagA.Start);
	}

	Buffer bs(&data, size);
	// 如果满了，马上发送
	if (bs.Length() > 256 - _Cache->Position())
		Report();
	_Cache->Write(bs);

	debug_printf("写入%d\r\n", Count);

	//写完数据后标致清零
	FlagA.Count = FlagB.Count = 0;
	Stop = false;
}

void Raster::Report()
{
	// 没数据或者客户端未登录
	if (_Cache->Length == 0 || Stop) return;
	//先把数据复制一下 
	Buffer bs(_Cache->GetBuffer(), _Cache->Position());

	MemoryStream cache;
	cache.Write(bs);
	_Cache->SetPosition(0);
	_Cache->Length = 0;

	//委托执行时间太久了
	OnReport(cache);

}
