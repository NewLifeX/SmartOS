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
	_Name	= "AlarmCf";
	_Start	= &Count;
	_End	= &TagEnd;
	Init();
}

/************************************************/

Alarm::Alarm()
{
	_taskid = 0;
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

	if ((item.Hour > 23 && item.Hour != 0xFF) || item.Minutes > 59 || item.Seconds > 59) return false;

	debug_printf("%d  %d  %d 执行 bs：", item.Hour, item.Minutes, item.Seconds);

	byte resid = SetCfg(item);
	result.Write(resid);

	return resid;
}

bool Alarm::Get(const Pair& args, Stream& result)
{
	AlarmConfig cfg;
	cfg.Load();

	result.Write(cfg.Count);
	// 注意，闹钟项可能不连续
	for (int i = 0; i < ArrayLength(cfg.Items); i++)
	{
		auto& item = cfg.Items[i];
		if (item.Index > 0)
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

	auto& dst	= cfg.Items[id - 1];
	if (item.Hour < 0xFF)
	{
		Buffer::Copy(&dst, &item, sizeof(item));
		//重新分配
		if (item.Index == 0) dst.Index	= id;
	}
	else
	{
		// 删除
		dst.Index	= 0;
	}

	// 重新计算个数
	int n = 0;
	for (int i = 0; i < ArrayLength(cfg.Items); i++)
	{
		if (cfg.Items[i].Index > 0)	n++;
	}

	cfg.Count	= n;
	cfg.Save();

	// 修改过后要检查一下Task的时间	// 取消下次动作并重新计算
	NextAlarmIds.Clear();
	Start();

	return id;
}

// 检查闹钟还有多少秒到期
static int CheckTime(const AlarmItem& item)
{
	// 判断有效
	if(!item.Enable) return -100;

	// 判断星期，0~6表示星期天到星期六
	auto now	= DateTime::Now();
	auto week	= now.DayOfWeek();
	int type	= item.Type.ToByte();
	if((type & (1 << week)) == 0) return -100;

	// 判断时间有效性
	auto dt	= now.Date();
	dt.Hour	= item.Hour;
	dt.Minute	= item.Minutes;
	dt.Second	= item.Seconds;

	// 需要特别小心时间的偏差
	return (dt - now).TotalSeconds();
}

void Alarm::AlarmTask()
{
	AlarmConfig cfg;
	cfg.Load();

	bool flag	= false;
	int next	= -1;
	// 遍历所有闹钟
	for (int i = 0; i < ArrayLength(cfg.Items); i++)
	{
		auto& item	= cfg.Items[i];
		// 检查闹钟还有多少秒到期
		int sec	= CheckTime(item);
		if (sec < 3 || sec > -3)
		{
			// 1长度 + 1类型 + n数据
			byte len = item.Data[0];
			if (len <= 10)
			{
				// 取动作类型
				auto type	= (int)item.Data[1];
				AlarmExecutor acttor;
				if (dic.TryGetValue(type, acttor))
				{
					// 取动作数据
					Buffer bs(&item.Data[2], len - 1);
					// 执行动作   DoSomething(item);
					acttor(type, bs);
				}
			}
			else
			{
				debug_printf("无效数据\r\n");
			}

			// 非重复闹铃需要禁用
			if(item.Type.Repeat)
			{
				item.Enable	= false;
				flag	= true;
			}
		}

		// 计算最近一次将要执行的时间
		if(sec > 3 && sec < next) next	= sec;
	}

	if(flag) cfg.Save();

	// 设置下次执行的时间
	if(next > 0) Sys.SetTask(_taskid, true, next);
}

void Alarm::Start()
{
	debug_printf("Alarm::Start\r\n");

	// 创建任务
	if (!_taskid)	_taskid	= Sys.AddTask(&Alarm::AlarmTask, this, 1000, 60000, "AlarmTask");

	// 马上调度一次
	Sys.SetTask(_taskid, true, 0);
}

void Alarm::Register(byte type, AlarmExecutor act)
{
	dic.Add((int)type, act);
}
