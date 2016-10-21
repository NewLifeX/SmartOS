#include "Raster.h"

static PulsePort* Create(Pin pin)
{
	auto pp	= new PulsePort();
	pp->Port = new InputPort(pin);
	pp->Port->HardEvent	= true;
	pp->Min	= 100;
	pp->Max	= 2000;

	return pp;
}

Raster::Raster(Pin pinA, Pin pinB, Pin bz)
{
	Init(bz);

	RasterA	= Create(pinA);
	RasterB	= Create(pinB);
}

Raster::~Raster()
{
	delete RasterA->Port;
	delete RasterA;
	delete RasterB->Port;
	delete RasterB;
}

void Raster::Init(Pin bz)
{
	AP = nullptr;
	Led = nullptr;
	RasterA = nullptr;
	RasterB = nullptr;
	Opened = false;

	Buzzer.Set(bz);
	Buzzer.Invert = 1;
	Buzzer.Blink(2, 100);
	//Buzzer.openDrain=true;
	Buzzer.Open();
	//BnkPort.Interval1	= 50;
	//BnkPort.Interval2	= 100;
	//BnkPort.Add(&Buzzer);
	//BnkPort.Count = 4;

	FlagTime = 3000;
	MinRTime = 500;
	// 光栅一直在轮训是否有数据要发送
	_task = Sys.AddTask(&Raster::Report, this, MinRTime, MinRTime, "延时发送任务");

	Sys.AddTask(&Raster::RestFlag, this, FlagTime, FlagTime, "光栅标志清零");
	// 缓冲区大小
	Cache.SetCapacity(255);
	byte arr[255];
	Buffer buf(arr, 255);
}

bool Raster::Open()
{
	if (Opened) return true;
	if (!RasterA || !RasterB)
	{
		debug_printf("光栅未赋值,打开失败.\r\n");
		return false;
	}

	if (RasterA != nullptr)
	{
		//Port->Register([](PulsePort* port, void* param) {((RasterOne*)(param))->OnHandler(port, param); }, this);
		RasterA->Press.Bind(&Raster::OnHandlerA, this);
		RasterA->Open();
		if (RasterA->Opened)
		{
			debug_printf("RasterA 初始化成功\r\n");
		}
		else
		{
			debug_printf("RasterA 初始化失败\r\n");
			delete RasterA;
			RasterA = nullptr;
			return false;
		}
	}

	if (RasterB != nullptr)
	{
		//Port->Register([](PulsePort* port, void* param) {((RasterOne*)(param))->OnHandler(port, param); }, this);
		RasterB->Press.Bind(&Raster::OnHandlerB, this);
		RasterB->Open();
		if (RasterB->Opened)
		{
			debug_printf("RasterB 初始化成功\r\n");
		}
		else
		{
			debug_printf("RasterB 初始化失败\r\n");
			delete RasterB;
			RasterB = nullptr;
			return false;
		}
	}

	Opened = true;
	return true;
}


void  Raster::RestFlag()
{
	auto time = Sys.Seconds() * 1000 + Sys.Ms() - Time.Milliseconds;

	if (FlagA.Count != 0)
	{
		if (time - FlagA.TriTime > FlagTime)
		{
			debug_printf("A超时清零\r\n");
			FlagA.Count = 0;
		}
	}
	if (FlagB.Count != 0)
	{
		if (time - FlagB.TriTime > FlagTime)
		{
			debug_printf("B超时清零\r\n");
			FlagB.Count = 0;
		}
	}
}

void Raster::OnHandlerA(PulsePort& raster)
{
	Buzzer.Blink(2, 500);
	debug_printf("A路触发，波长%d \r\n", raster.Time);
	FlagA.TriTime = raster.Last - raster.Time;
	FlagA.Occlusion = raster.Time;

	if (!FlagB.Count)
	{
		FlagA.Count++;
		return;
	}

	Count++;

	LineReport();
}

void Raster::OnHandlerB(PulsePort& raster)
{
	Buzzer.Blink(2, 500);
	debug_printf("B路触发，波长%d \r\n", raster.Time);
	FlagB.TriTime = raster.Last - raster.Time;
	FlagB.Occlusion = raster.Time;

	if (!FlagA.Count)
	{
		FlagB.Count++;
		return;
	}

	Count++;
	LineReport();
}

void  Raster::Report()
{
	//没数据或者客户端未登录
	if (Cache.Position() == 0 || AP->GetStatus() != 2)return;
	if (Cache.Position() == 0) return;

	Buffer bs(Cache.GetBuffer(), Cache.Position());
	if (Led)Led->Write(2);
	MemoryStream ms;
	BinaryPair bp(ms);
	bp.Set("Data", bs);
	debug_printf("上报数据: ");
	bs.Show();
	AP->Invoke("Raster/RasterUpt", Buffer(ms.GetBuffer(), ms.Position()));
	Cache.SetPosition(0);

	if (Led)Led->Write(2);
}

void  Raster::LineReport()
{
	auto size = sizeof(RasTriData);

	RasTriData data;

	data.Index = Index;
	data.TriTime = DateTime::Now().TotalMs();
	data.OcclusionA = FlagA.Occlusion;
	data.OcclusionB = FlagB.Occlusion;
	data.Count = Count;

	//时间差加方向
	if (FlagA.TriTime > FlagB.TriTime)
	{
		data.Direction = 0x00;
		data.Interval = ushort(FlagA.TriTime - FlagB.TriTime);
	}
	else
	{
		data.Direction = 0x01;
		data.Interval = ushort(FlagB.TriTime - FlagA.TriTime);
	}

	Buffer bs(&data, size);
	auto point = Cache.Position();

	auto len = 2 + size * 8 - point;

	//写数据停止调度,并且告诉外部该函数已经被占用
	ReadyCache = false;

	if (len <= size + 2)
	{
		//Cache.Write(bs);
		debug_printf("缓冲区满\r\n");
		//写完数据后标致清零
		FlagA.Count = FlagB.Count = 0;
		ReadyCache = true;
		Sys.SetTask(_task, true, 0);
		// 缓冲区满了，马上发送
		return;
	}
	//包数
	byte count = 0;
	if (point != 0)
	{
		Cache.SetPosition(0);
		count = Cache.ReadByte();
	}
	count = count++;

	Cache.SetPosition(0);
	// 写入个数
	Cache.Write(count);

	if (point == 0)
	{
		point = 1;
	}
	// 还原游标
	Cache.SetPosition(point);
	// 数据写入缓存
	Cache.Write(bs);
	ReadyCache = true;
	//写完数据后标致清零
	FlagA.Count = FlagB.Count = 0;
	debug_printf("缓存%d条数据 \r\n", count);
}

