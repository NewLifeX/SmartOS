#include "Alarm.h"
#include "Kernel\TTime.h"

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

	byte id = SetCfg(item);
	result.Write(id);

	// 马上调度一次
	if (id) Sys.SetTask(_taskid, true, 0);

	return id > 0;
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

	auto& dst = cfg.Items[id - 1];

	if (item.Hour < 0xFF)
	{
		Buffer::Copy(&dst, &item, sizeof(item));
		//重新分配
		if (item.Index == 0) dst.Index = id;
	}
	else
	{
		// 删除
		dst.Index = 0;
	}

	// 重新计算个数
	int n = 0;
	for (int i = 0; i < ArrayLength(cfg.Items); i++)
	{
		if (cfg.Items[i].Index > 0)	n++;
	}

	cfg.Count = n;
	cfg.Save();

	return id;
}

// 检查闹钟还有多少秒到期
static int CheckTime(const AlarmItem& item)
{
	// 判断有效
	if (!item.Enable) return -100;

	// 判断星期，0~6表示星期天到星期六
	auto now = DateTime::Now();
	auto dt = now.Date();
	auto week = now.DayOfWeek();

	int type = item.Type.ToByte();

	// 今天星期是否配对
	if ((type & (1 << week)) == 0)
	{
		// 明天星期是否配对
		if (++week >= 7) week -= 7;
		if ((type & (1 << week)) == 0)  return -100;

		// 明天时间减去现在时间，避免漏了刚好跨天的闹铃
		dt.Day++;
	}

	// 判断时间有效性
	dt.Hour = item.Hour;
	dt.Minute = item.Minutes;
	dt.Second = item.Seconds;

	// 需要特别小心时间的偏差
	return (dt - now).TotalSeconds();
}

void Alarm::AlarmTask()
{
	// 如果系统年份不对，则不执行任何动作
	auto now = DateTime::Now();
	if (now.Year < 2010) return;

	AlarmConfig cfg;
	cfg.Load();

	bool flag = false;
	int next = -1;
	// 遍历所有闹钟
	for (int i = 0; i < ArrayLength(cfg.Items); i++)
	{
		auto& item = cfg.Items[i];
		if (item.Index == 0) continue;

		// 检查闹钟还有多少秒到期
		int sec = CheckTime(item);
		if (sec <= 1 && sec >= -2)
		{
			debug_printf("闹钟执行 %02d:%02d:%02d\r\n", item.Hour, item.Minutes, item.Seconds);

			OnAlarm(item);
		}

		// 计算最近一次将要执行的时间
		if (sec > 1 && (next < 0 || sec < next)) next = sec;
	}

	if (flag) cfg.Save();
	if (next > 0)
		Sys.SetTask(_taskid, true, next * 1000);
}

void Alarm::Start()
{
	debug_printf("Alarm::Start\r\n");

	// 创建任务
	if (!_taskid)
		_taskid = Sys.AddTask(&Alarm::AlarmTask, this, 1000, 60000, "定时闹钟");
	else
		// 马上调度一次
		Sys.SetTask(_taskid, true, 0);
}

void Alarm::Test()
{
	debug_printf("闹钟测试...\r\n");

	Alarm am;
	for (size_t i = 0; i < 7; i++)
	{
		AlarmItem item;
		item.Enable = true;
		item.Hour = 23;
		item.Index = 0;
		item.Minutes = 59;
		item.Seconds = 57;
		item.Type.Init((byte)(1 << i));
		am.SetCfg(item);
	}

	auto now = DateTime::Now();
	auto dt = now.Date();

	dt.Hour = 23;
	dt.Minute = 59;
	dt.Second = 50;

	//时间设定好
	((TTime&)Time).SetTime(dt.TotalSeconds() - 60);

	debug_printf("初始化设定时间\r\n");
	DateTime::Now().Show(true);
	//test = true;
}
