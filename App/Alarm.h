#ifndef __Alarm_H__
#define __Alarm_H__

#include "Sys.h"
#include "Device\Timer.h"
#include "Device\Port.h"
#include "Config.h"
#include "Message\BinaryPair.h"
#include "Message\DataStore.h"

/*
注册给 TokenClient 名称 Policy/Set
注册给 TokenClient 名称 Policy/Get

Action = Policy/Set   alarm = AlarmItem
Action = Policy/Get


Set
1. AlarmItem.number = 0 的时候 自动选择Enable = false的编号进行存储
		否则 按照number进行储存和执行。
2. AlarmItem.Data 为定时器需要执行命令的数据。
3. AlarmItem.Data   =   (1byte)len + (1byte)type + (len-1 byte)data[]
		type 为执行动作的类型，不同类型有不同的操作函数。 type跟函数 在bsp里进行注册。

Get
  直接一次性返回所有（20条）定时动作的数据。（包含编号）。

*/

// 执行定时器的函数类型
typedef void(*AlarmExecutor)(byte type, Buffer& bs);

typedef struct
{
	byte Sunday : 1;
	byte Monday : 1;
	byte Tuesday : 1;
	byte Wednesday : 1;
	byte Thursday : 1;
	byte Friday : 1;
	byte Saturday : 1;
	byte Repeat : 1;
public:
	void Init(byte data = 0) { *(byte*)this = data; }
	byte ToByte() const { return *(byte*)this; }
}AlarmType;

// 必须设定为1字节对齐，否则offsetof会得到错误的位置
#pragma pack(push)	// 保存对齐状态
// 强制结构体紧凑分配空间
#pragma pack(1)
//
typedef struct
{
	byte Index;		// 闹钟编号
	byte Enable;	// 是否有效
	AlarmType Type;	// week相关
	byte Hour;		// 时
	byte Minutes;	// 分
	byte Seconds;	// 秒
	byte Data[10];	// 第一个字节 有效数据长度，第二个字节动作类型，后面是数据
}AlarmItem;
#pragma pack(pop)	// 恢复对齐状态

class Alarm
{
public:
	Alarm();

	/*  注册给 TokenClient 名称 Policy/Set  */
	bool Set(const Pair& args, Stream& result);
	/*  注册给 TokenClient 名称 Policy/Get  */
	bool Get(const Pair& args, Stream& result);
	void Start();
	// 注册各种类型的执行动作
	void Register(byte type, AlarmExecutor act);

	 void Test();

private:
	Dictionary<int, AlarmExecutor> dic;// AlarmItem.Data[1] 表示动作类型，由此字典进行匹配动作执行器

	uint		_taskid;			// 闹钟TaskId
	void AlarmTask();

	// Config
	byte SetCfg(const AlarmItem& item) const;
};


#endif
