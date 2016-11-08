#include "Alarm.h"

#define Int_Max  2147483647

class AlarmConfig :public ConfigBase
{
public:
	byte Count;
	AlarmItem Items[20];
	byte TagEnd;

	AlarmConfig();
};

AlarmConfig::AlarmConfig()
{
	_Name = "AlarmCf";
	_Start = &Count;
	_End = &TagEnd;
	Init();
}

/************************************************/

Alarm::Alarm()
{
	AlarmTaskId = 0;
}

bool Alarm::Set(const Pair& args, Stream& result)
{
	debug_printf("Set\r\n");

	Buffer buf = args.Get("alarm");
	if (buf.Length() < 7 || buf[0] > 20)
	{
		debug_printf("数据有误\r\n");
		result.Write((byte)0);
		return false;
	}

	auto& item = *(AlarmItem*)buf.GetBuffer();

	if (item.Hour > 23 && item.Hour != 0xFF || item.Minutes > 59 || item.Seconds > 59) return false;

	debug_printf("%d  %d  %d 执行 bs：", item.Hour, item.Minutes, item.Seconds);

	byte resid = SetCfg(item);
	result.Write(resid);

	return resid;
}

bool Alarm::Get(const Pair& args, Stream& result) const
{
	AlarmConfig cfg;
	cfg.Load();

	result.Write(cfg.Count);
	// ZhuYi BuLianXu
	for (int i = 0; i < ArrayLength(cfg.Items); i++)
	{
		auto& item	= cfg.Items[i];
		if(item.Index > 0)
		{
			result.WriteArray(Buffer(&item, sizeof(item)));
		}
	}

	return true;
}

byte Alarm::SetCfg(const AlarmItem& item)
{
	AlarmConfig cfg;
	cfg.Load();

	byte id = item.Index;
	if (!id)	// 找到空闲位置
	{
		for (int i = 0; i < ArrayLength(cfg.Items); i++)
		{
			if (cfg.Items[i].Index == 0)
			{
				id = i + 1;
				break;
			}
		}
	}
	if (!id) return 0;	// 查找失败

	if(item.Hour < 0xFF)
		Buffer::Copy(&cfg.Items[id - 1], &item, sizeof(item));
	else	// Delete
		cfg.Items[id - 1].Index	= 0;

	int n	= 0;
	for (int i = 0; i < ArrayLength(cfg.Items); i++)
	{
		if(cfg.Items[i].Index > 0) n++;
	}

	cfg.Count	= n;
	cfg.Save();

	// 修改过后要检查一下Task的时间	// 取消下次动作并重新计算
	NextAlarmIds.Clear();
	Start();

	return id;
}

bool Alarm::GetCfg(byte id, AlarmItem& item)
{
	AlarmConfig cfg;
	cfg.Load();

	Buffer bf(&item.Index, sizeof(AlarmItem));
	Buffer bf2(&cfg.Items[id].Index, sizeof(AlarmItem));
	bf = bf2;
	return true;
}

int Alarm::CalcNextTime(AlarmItem& item)
{
	debug_printf("CalcNextTime Id %d  ",item.Index);
	auto now = DateTime::Now();
	byte type = item.Type.ToByte();
	byte week = now.DayOfWeek();
	int time;
	if (type & 1 << week)	// 今天
	{
		DateTime dt(now.Year, now.Month, now.Day);
		dt.Hour = item.Hour;
		dt.Minute = item.Minutes;
		dt.Second = item.Seconds;
		if (dt > now)
		{
			time = (dt - now).Ms;
			debug_printf("%d\r\n", time);
			return time;		// 今天闹钟还没响
		}
	}
	debug_printf("after today\r\n");
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

	int times[ArrayLength(cfg.Items)];
	for (int i = 0; i < ArrayLength(cfg.Items); i++)
	{
		times[i] = Int_Max;
		if (!cfg.Items[i].Enable)continue;
		int time = CalcNextTime(cfg.Items[i]);	// 但凡有效的都计算出来
		times[i] = time;

		if (time < miniTime)miniTime = time;	// 找出最小时间
	}

	NextAlarmIds.Clear();
	if (miniTime != Int_Max)
	{
		for (int i = 0; i < ArrayLength(cfg.Items); i++)
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
		nextTime = tomorrowTime;
		debug_printf("今天没有闹钟任务，下次唤醒时间为明天\r\n");
	}

	return NextAlarmIds.Count();
}

void Alarm::AlarmTask()
{
	// 获取定时的数据
	AlarmItem item;
	// 拿到现在的时间
	auto now = DateTime::Now();
	now.Ms = 0;
	for (int i = 0; i < NextAlarmIds.Count(); i++)
	{
		byte NextAlarmId = NextAlarmIds[i];
		GetCfg(NextAlarmId, item);
		// 拿到定时项的时间
		DateTime dt(now.Year, now.Month, now.Day);
		dt.Hour = item.Hour;
		dt.Minute = item.Minutes;
		dt.Second = dt.Second;
		dt.Ms = 0;
		// 对比现在时间和定时项的时间  符合则执行任务
		if (dt == now)
		{
			// 第一个字节 有效数据长度，第二个字节动作类型，后面是数据
			// 取总体数据长度
			byte len = item.Data[0];
			if (len <= 10)
			{
				// 取动作类型
				auto type = (int)item.Data[1];
				AlarmExecutor acttor;
				if (dic.TryGetValue(type, acttor))
				{
					// 取动作数据
					Buffer bs(&item.Data[2], len - 1);
					// 执行动作   DoSomething(item);
					acttor(NextAlarmId, bs);
				}
			}
			else
			{
				debug_printf("无效数据\r\n");
			}
		}
	}

	// 找到下一个定时器动作的时间，并拿到距离下次执行的时间。如果今天没有下一次了 就定明天早上第一时间来计算一次。
	FindNext(NextAlarmMs);
	// 设置下次执行的时间
	Sys.SetTask(AlarmTaskId, true, NextAlarmMs);
}

void Alarm::Start()
{
	debug_printf("Alarm::Start\r\n");
	if (!AlarmTaskId)AlarmTaskId = Sys.AddTask(&Alarm::AlarmTask, this, -1, -1, "AlarmTask");
	Sys.SetTask(AlarmTaskId, true, 0);
}

void Alarm::Register(byte type, AlarmExecutor act)
{
	dic.Add((int)type, act);
}
