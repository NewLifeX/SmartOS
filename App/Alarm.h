#ifndef __Alarm_H__
#define __Alarm_H__

#include "Sys.h"
#include "Timer.h"
#include "Port.h"
#include "Config.h"
#include "Message\BinaryPair.h"
#include "Message\DataStore.h"

/*
注册给 TokenClient 名称 Policy/AlarmSet
注册给 TokenClient 名称 Policy/AlarmGet

Action = Policy/AlarmSet   alarm = AlarmDataType
Action = Policy/AlarmGet


AlarmSet
1. AlarmDataType.number = 0 的时候 自动选择Enable = false的编号进行存储
		否则 按照number进行储存和执行。
2. AlarmDataType.Data 为定时器需要执行命令的数据。
3. AlarmDataType.Data   =   (1byte)len + (1byte)type + (len-1 byte)data[]
		type 为执行动作的类型，不同类型有不同的操作函数。 type跟函数 在bsp里进行注册。

AlarmGet
  直接一次性返回所有（20条）定时动作的数据。（包含编号）。

*/

// 执行定时器的函数类型
typedef void(*AlarmActuator)(byte type, Buffer& bs);

typedef struct
{
	byte Sunday		: 1;
	byte Monday		: 1;
	byte Tuesday	: 1;
	byte Wednesday	: 1;
	byte Thursday	: 1;
	byte Friday		: 1;
	byte Saturday	: 1;
	byte Repeat : 1;
public:
	void Init(byte data = 0) { *(byte*)this = data; }
	byte ToByte() { return *(byte*)this; }
}AlarmType;

// 必须设定为1字节对齐，否则offsetof会得到错误的位置
#pragma pack(push)	// 保存对齐状态
// 强制结构体紧凑分配空间
#pragma pack(1)
typedef struct
{
	byte Number;	// 闹钟编号
	byte Enable;	// 是否有效
	AlarmType Type;	// week相关
	byte Hour;		// 时
	byte Minutes;	// 分
	byte Seconds;	// 秒
	byte Data[11];	// 第一个字节 有效数据长度，第二个字节动作类型，后面是数据
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
	// Config
	byte SetCfg(byte id, AlarmDataType& data);
	bool GetCfg(byte id, AlarmDataType& data);

	void Start();
	// 注册执行动作的函数
	void Register(byte type, AlarmActuator act);

private:
	Dictionary<int, AlarmActuator> dic;// AlarmDataType.Data[1] 表示动作类型，由此字典进行匹配动作执行器

	uint AlarmTaskId;			// 闹钟TaskId
	List<int>NextAlarmIds;		// 下次运行的编号，允许多组定时器定时时间相同
	int NextAlarmMs;			// 下次闹钟时间
	// Task
	void AlarmTask();
	// 找到最近的闹钟时间的id以及时间
	byte FindNext(int& nextTime);
	// 计算下次闹钟时间
	int CalcNextTime(AlarmDataType& data);
};


#endif 
