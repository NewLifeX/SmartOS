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

byte Alarm::SetCfg(const AlarmItem& item) const
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

	// 马上调度一次
	//Sys.SetTask(_taskid, true, 0);

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

	//now.Show();

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
	dt.Hour		= item.Hour;
	dt.Minute	= item.Minutes;
	dt.Second	= item.Seconds;

	// 需要特别小心时间的偏差
	 return (dt - now).TotalSeconds();
}
static bool test = false;
void Alarm::AlarmTask()
{
	// 如果系统年份不对，则不执行任何动作
	auto now = DateTime::Now();
	if (now.Year < 2010) return;

	//if (!test)
	//{
	//	Test();
	//}
	AlarmConfig cfg;
	cfg.Load();

	bool flag = false;
	int next = 60000;
	// 遍历所有闹钟
	for (int i = 0; i < ArrayLength(cfg.Items); i++)
	{
		auto& item = cfg.Items[i];
		if (item.Index == 0) continue;

		// 检查闹钟还有多少秒到期
		int sec = CheckTime(item);

		debug_printf("\r\n");
		debug_printf("sec = %d\r\n", sec);
		if (sec < 3 && sec > -3)
		{
			debug_printf("闹钟执行系统时间:");
			now.Show();
			debug_printf(" 星期:%d", now.DayOfWeek());
			debug_printf("闹钟时间 %d:%d:%d\r\n", item.Hour, item.Minutes, item.Seconds);

			// 1长度 + 1类型 + n数据
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
					acttor(type, bs);
				}
			}
			else
			{
				debug_printf("无效数据\r\n");
			}

			debug_printf("\r\n");
			//// 非重复闹铃需要禁用
			//if(item.Type.Repeat)
			//{
			//	item.Enable	= false;
			//	flag	= true;
			//}

			//auto dt = now.Date();
			//dt.Hour = 23;
			//dt.Minute = 59;
			//dt.Second = 50;
			//dt.Day++;

			//((TTime&)Time).SetTime(dt.TotalSeconds() - 3);
			//debug_printf("设定系统时间:");
			//DateTime::Now().Show();
		}

		// 计算最近一次将要执行的时间
		if (sec >= 3 && sec < next) next = sec;
	}

	debug_printf("下次执行时间%d\r\n", next);
	if (flag) cfg.Save();	
	if (next > 0)
	    Sys.SetTask(_taskid, true, next*1000);
}

void Alarm::Start()
{
	debug_printf("Alarm::Start\r\n");

	// 创建任务
	if (!_taskid)	_taskid = Sys.AddTask(&Alarm::AlarmTask, this, 1000, 20000, "AlarmTask");

	// 马上调度一次
	Sys.SetTask(_taskid, true, 0);
}

void Alarm::Register(byte type, AlarmExecutor act)
{
	dic.Add((int)type, act);
}

void Alarm::Test()
{
	debug_printf("闹钟测试...\r\n");

	for (size_t i = 0; i < 7; i++)
	{
		AlarmItem item;
		item.Enable = true;
		item.Hour = 23;
		item.Index = 0;
		item.Minutes = 59;
		item.Seconds = 57;
		item.Type.Init((byte)(1 << i));
		SetCfg(item);
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
	test = true;
}
