#include "Alarm.h"

#define Int_Max  2147483647

class AlarmConfig :public ConfigBase
{
public:
	byte Length;
	AlarmDataType Data[20];
	byte TagEnd;

	AlarmConfig();
	virtual void Init();
};

AlarmConfig::AlarmConfig()
{
	_Name = "AlarmCf";
	_Start = &Length;
	_End = &TagEnd;
	Init();
}

void AlarmConfig::Init()
{
	Buffer(Data, sizeof(Data)).Clear();
}

/************************************************/

Alarm::Alarm()
{
	AlarmTaskId = 0;
}

bool Alarm::AlarmSet(const Pair& args, Stream& result)
{
	AlarmDataType data;
	data.Enable = false;

	Buffer buf = args.Get("Data");
	Stream ms(buf);

	byte Id = 0xff;
	Id = ms.ReadByte();
	if (Id > 20)return false;

	data.Enable = ms.ReadByte();

	byte type = ms.ReadByte();
	data.Type.Init(type);

	data.Hour = ms.ReadByte();
	data.Minutes = ms.ReadByte();
	data.Seconds = ms.ReadByte();

	if (data.Hour > 23 || data.Minutes > 59 || data.Seconds > 59)return false;

	Buffer buf2(data.Data, sizeof(data.Data));
	buf2 = ms.ReadBytes();

	return SetCfg(Id, data);
}

bool Alarm::AlarmGet(const Pair& args, Stream& result)
{
	AlarmConfig cfg;
	cfg.Load();

	result.Write((byte)ArrayLength(cfg.Data));		// 写入长度
	for (AlarmDataType &x : cfg.Data)
	{
		Buffer bs(&x.Enable, sizeof(AlarmDataType));
		result.WriteArray(bs);
	}

	return true;
}

bool Alarm::SetCfg(byte id, AlarmDataType& data)
{
	AlarmConfig cfg;
	cfg.Load();

	Buffer bf(&data, sizeof(AlarmDataType));
	Buffer bf2(&cfg.Data[id].Enable, sizeof(AlarmDataType));
	bf2 = bf;

	cfg.Save();
	// 修改过后要检查一下Task的时间	// 取消下次动作并重新计算
	NextAlarmIds.Clear();
	Start();
	return true;
}

bool Alarm::GetCfg(byte id, AlarmDataType& data)
{
	AlarmConfig cfg;
	cfg.Load();

	Buffer bf(&data, sizeof(AlarmDataType));
	Buffer bf2(&cfg.Data[id].Enable, sizeof(AlarmDataType));
	bf = bf2;
	return true;
}

int Alarm::CalcNextTime(AlarmDataType& data)
{
	debug_printf("CalcNextTime :");
	auto now = DateTime::Now();
	byte type = data.Type.ToByte();
	byte week = now.DayOfWeek();
	int time;
	if (type & 1 << week)	// 今天
	{
		DateTime dt(now.Year, now.Month, now.Day);
		dt.Hour = data.Hour;
		dt.Minute = data.Minutes;
		dt.Second = data.Seconds;
		if (dt > now)
		{
			time = (dt - now).Ms;
			debug_printf("%d\r\n", time);
			return time;		// 今天闹钟还没响
		}
	}
	debug_printf("max\r\n");
	return Int_Max;
}

int ToTomorrow()
{
	auto dt = DateTime::Now();
	int time = (24 - dt.Hour - 1) * 3600000;		// 时-1  ->  ms
	time += ((60 - dt.Minute - 1) * 60000);			// 分-1  ->  ms
	time += ((60 - dt.Second) * 1000);				// 秒	 ->  ms
	debug_printf("ToTomorrow : %d\r\n", time);
	return time;
}

byte Alarm::FindNext(int& nextTime)
{
	debug_printf("FindNext\r\n");
	AlarmConfig cfg;
	cfg.Load();

	int miniTime = Int_Max;
	int tomorrowTime = ToTomorrow();

	int times[ArrayLength(cfg.Data)];
	for (int i = 0; i < ArrayLength(cfg.Data); i++)
	{
		times[i] = Int_Max;
		if (!cfg.Data[i].Enable)continue;
		int time = CalcNextTime(cfg.Data[i]);	// 但凡有效的都计算出来
		times[i] = time;

		if (time < miniTime)miniTime = time;	// 找出最小时间
	}

	if (miniTime != Int_Max)
	{
		for (int i = 0; i < ArrayLength(cfg.Data); i++)
		{
			if (times[i] == miniTime)
				NextAlarmIds.Add(i);
		}
		nextTime = miniTime;
	}
	else
	{
		// 如果最小值无效   直接明早再来算一次
		// nextTime = ToTomorrow();
		nextTime = tomorrowTime;
		NextAlarmIds.Clear();
	}

	return NextAlarmIds.Count();
}

void Alarm::AlarmTask()
{
	debug_printf("AlarmTask");
	// 获取定时的数据
	AlarmDataType data;
	auto now = DateTime::Now();
	now.Ms = 0;
	for (int i = 0; i < NextAlarmIds.Count(); i++)
	{
		byte NextAlarmId = NextAlarmIds[0];
		GetCfg(NextAlarmId, data);

		DateTime dt(now.Year, now.Month, now.Day);
		dt.Hour = data.Hour;
		dt.Minute = data.Minutes;
		dt.Second = dt.Second;
		dt.Ms = 0;
		if (dt == now)
		{
			// 执行动作   DoSomething(data);
			debug_printf("  DoSomething:   ");
			ByteArray bs((const void*)data.Data, sizeof(data.Data));
			bs.Show(true);
		}
	}
	NextAlarmIds.Clear();

	// 找到下一个定时器动作的时间
	FindNext(NextAlarmMs);
	if (NextAlarmIds.Count() != 0)
		Sys.SetTask(AlarmTaskId, true, NextAlarmMs);
	else
		Sys.SetTask(AlarmTaskId, false);
}

void Alarm::Start()
{
	debug_printf("Alarm::Start\r\n");
	if (!AlarmTaskId)AlarmTaskId = Sys.AddTask(&Alarm::AlarmTask, this, -1, -1, "AlarmTask");
	Sys.SetTask(AlarmTaskId, true);
}
