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
	for (int i = 0; i < ArrayLength(Data); i++)
	{
		Data[i].Number = i + 1;
	}
}

/************************************************/

Alarm::Alarm()
{
	AlarmTaskId = 0;
}

bool Alarm::AlarmSet(const Pair& args, Stream& result)
{
	debug_printf("AlarmSet\r\n");
	AlarmDataType alarm;
	alarm.Enable = false;

	Buffer buf = args.Get("alarm");

	Stream ms(buf);
	if (buf.Length() < 7)
	{
		debug_printf("数据有误\r\n");
		result.Write((byte)0);
		return false;
	}

	byte Id = 0xff;
	Id = ms.ReadByte();
	if (Id > 20)
	{
		debug_printf("Index有误\r\n");
		result.Write((byte)0);
		return false;
	}
	alarm.Number = Id;

	alarm.Enable = ms.ReadByte();

	byte type = ms.ReadByte();
	alarm.Type.Init(type);

	alarm.Hour = ms.ReadByte();
	alarm.Minutes = ms.ReadByte();
	alarm.Seconds = ms.ReadByte();

	if (alarm.Hour > 23 || alarm.Minutes > 59 || alarm.Seconds > 59)return false;

	// Buffer buf2(data.Data, sizeof(data.Data));
	// auto len = ms.ReadArray(buf2);

	Buffer buf2(alarm.Data, sizeof(alarm.Data));
	Buffer buf3(buf.GetBuffer() + ms.Position(), buf.Length() - ms.Position());
	buf2 = buf3;

	// buf.Show(true);
	// buf3.Show(true);
	debug_printf("%d  %d  %d 执行 bs：", alarm.Hour, alarm.Minutes, alarm.Seconds);
	buf2.Show(true);

	byte resid = SetCfg(Id, alarm);
	result.Write((byte)resid);
	if (resid)return true;
	return false;
}

bool Alarm::AlarmGet(const Pair& args, Stream& result)
{
	// debug_printf("AlarmGet");
	AlarmConfig cfg;
	cfg.Load();
	// debug_printf("data :\r\n");
	result.Write((byte)20);		// 写入长度
	for (int i = 0; i < 20; i++)
	{
		Buffer bs(&cfg.Data[i].Number, sizeof(AlarmDataType));
		bs.Show(true);
		result.WriteArray(bs);
	}

	Buffer(result.GetBuffer(), result.Position()).Show(true);
	// debug_printf("\r\n");
	return true;
}

byte Alarm::SetCfg(byte id, AlarmDataType& data)
{
	AlarmConfig cfg;
	cfg.Load();

	if (!id)	// 找到空闲位置
	{
		for (int i = 0; i < 20; i++)
		{
			if (!cfg.Data[i].Enable)
			{
				id = i + 1;
				break;
			}
		}
	}
	if (!id)return 0;	// 查找失败

	Buffer bf(&data.Number, sizeof(AlarmDataType));
	Buffer bf2(&cfg.Data[id - 1].Number, sizeof(AlarmDataType));
	bf2 = bf;

	for (int i = 0; i < 20; i++)
	{
		cfg.Data[i].Number = i + 1;		// 避免 id 出错
	}

	cfg.Save();
	// 修改过后要检查一下Task的时间	// 取消下次动作并重新计算
	NextAlarmIds.Clear();
	Start();
	return id;
}

bool Alarm::GetCfg(byte id, AlarmDataType& data)
{
	AlarmConfig cfg;
	cfg.Load();

	Buffer bf(&data.Number, sizeof(AlarmDataType));
	Buffer bf2(&cfg.Data[id].Number, sizeof(AlarmDataType));
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
	// debug_printf("ToTomorrow : %d\r\n", time);
	return time;
}

byte Alarm::FindNext(int& nextTime)
{
	// debug_printf("FindNext\r\n");
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
			{
				NextAlarmIds.Add(i);
				debug_printf("添加下一次闹钟的id %d\r\n", i);
			}
		}
		nextTime = miniTime;
		debug_printf("下一个闹钟时间是%dMs后\r\n", nextTime);
	}
	else
	{
		// 如果最小值无效   直接明早再来算一次
		// nextTime = ToTomorrow();
		nextTime = tomorrowTime;
		debug_printf("今天没有闹钟任务，下次唤醒时间为明天\r\n");
		NextAlarmIds.Clear();
	}

	return NextAlarmIds.Count();
}

void Alarm::AlarmTask()
{
	// debug_printf("AlarmTask");
	// 获取定时的数据
	AlarmDataType data;
	auto now = DateTime::Now();
	now.Ms = 0;
	for (int i = 0; i < NextAlarmIds.Count(); i++)
	{
		byte NextAlarmId = NextAlarmIds[i];
		GetCfg(NextAlarmId, data);

		DateTime dt(now.Year, now.Month, now.Day);
		dt.Hour = data.Hour;
		dt.Minute = data.Minutes;
		dt.Second = dt.Second;
		dt.Ms = 0;
		if (dt == now)
		{
			// 第一个字节 有效数据长度，第二个字节动作类型，后面是数据
			byte len = data.Data[0];
			if (len <= 10)
			{
				auto type = (int)data.Data[1];
				AlarmActuator acttor;
				if (dic.TryGetValue(type, acttor))
				{
					Buffer bs(&data.Data[2], len - 1);
					// 执行动作   DoSomething(data);
					// debug_printf("  DoSomething type %d ",type);
					// bs.Show(true);
					acttor(NextAlarmId, bs);
				}
			}
			else
			{
				debug_printf("无效数据\r\n");
			}
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
	Sys.SetTask(AlarmTaskId, true, 0);
}

void Alarm::Register(byte type, AlarmActuator act)
{
	dic.Add((int)type, act);
}
