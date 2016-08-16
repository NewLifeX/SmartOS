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
	AfterAlarmId = 0xff;
}

bool Alarm::SetCfg(byte id, AlarmDataType& data)
{
	AlarmConfig cfg;
	cfg.Load();

	Buffer bf(&data, sizeof(AlarmDataType));
	Buffer bf2(&cfg.Data[id].Enable, sizeof(AlarmDataType));
	bf2 = bf;

	cfg.Save();
	// 修改过后要检查一下Task的时间
	// xxxx
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
	auto dt = DateTime::Now();
	if (data.Type.ToByte() & 1 << dt.DayOfWeek())
	{

	}
	return Int_Max;
}

byte Alarm::FindAfter()
{
	AlarmConfig cfg;
	cfg.Load();

	int nextTimeMs;
	byte id = 0xff;
	for (int i = 0; i < 20; i++)
	{
		if (!cfg.Data[i].Enable)continue;
		int time = CalcNextTime(cfg.Data[i]);
		if (time < nextTimeMs) 
		{
			nextTimeMs = time;
			id = i;
		}
	}
	if (id != 0xff)NextAlarmMs = nextTimeMs;

	return id;
}

void Alarm::AlarmTask()
{
	// 执行动作

	// 找到下一个定时器动作的时间
	AfterAlarmId = FindAfter();
	if (AfterAlarmId != 0xff)
	{
		Sys.SetTask(AlarmTaskId, true, NextAlarmMs);
		return;
	}	

	Sys.SetTask(AlarmTaskId, false);
}

void Alarm::Start()
{
	if (!AlarmTaskId)AlarmTaskId = Sys.AddTask(&Alarm::AlarmTask, this, -1, -1, "AlarmTask");
	Sys.SetTask(AlarmTaskId,true);
}

