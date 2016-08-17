﻿#ifndef __Alarm_H__
#define __Alarm_H__

#include "Sys.h"
#include "Timer.h"
#include "Port.h"
#include "..\Config.h"
#include "Message\BinaryPair.h"

class ByteStruct2	// 位域基类
{
public:
	void Init(byte data = 0) { *(byte*)this = data; }
	byte ToByte() { return *(byte*)this; }
};

typedef struct : ByteStruct2
{
	byte Sunday		: 1;
	byte Monday		: 1;
	byte Tuesday	: 1;
	byte Wednesday	: 1;
	byte Thursday	: 1;
	byte Friday		: 1;
	byte Saturday	: 1;
	byte Repeat		: 1;
}AlarmType;

// 必须设定为1字节对齐，否则offsetof会得到错误的位置
#pragma pack(push)	// 保存对齐状态
// 强制结构体紧凑分配空间
#pragma pack(1)
typedef struct
{
	byte Enable;
	AlarmType Type;
	byte Hour;
	byte Minutes;
	byte Seconds;
	byte Data[11];
}AlarmDataType;
#pragma pack(pop)	// 恢复对齐状态

class Alarm
{
public:
	Alarm();
	
	/*  注册给 TokenClient 名称 Policy/AlarmSet  */
	bool AlarmSet(const Pair& args, Stream& result);
	/*  注册给 TokenClient 名称 Policy/AlarmGet  */
	bool AlarmGet(const Pair& args, Stream& result);

	bool SetCfg(byte id, AlarmDataType& data);
	bool GetCfg(byte id, AlarmDataType& data);

	void Start();

private:
	uint AlarmTaskId;
	List<int>NextAlarmIds;		// 下次运行的编号，允许多组定时器定时时间相同
	int NextAlarmMs;			// 下次闹钟时间

	void AlarmTask();
	byte FindNext(int& nextTime);
	int CalcNextTime(AlarmDataType& data);
};

// class AlarmActivity
// {
// public:
// 	DataStore* store;
// 
// 
// 
// };

#endif 
