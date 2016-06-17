#include "Sys.h"

#include "Time.h"
//#include "WatchDog.h"

//#include "Platform\stm32.h"

TSys Sys;
const TTime Time;

#if defined(BOOT) || defined(APP)

//#pragma location  = 0x20000000
struct HandlerRemap StrBoot __attribute__((at(0x2000fff0)));

#endif

extern uint __heap_base;
extern uint __heap_limit;
extern uint __initial_sp;
extern uint __microlib_freelist;
extern uint __microlib_freelist_initialised;

#ifndef BIT
    #define BIT(x)	(1 << (x))
#endif

#if !defined(TINY) && defined(STM32F0)
	#pragma arm section code = "SectionForSys"
#endif

TSys::TSys()
{
	Config	= nullptr;

	OnInit();

	OnSleep	= nullptr;

	Code	= 0x0000;
	Ver		= 0x0300;
#ifndef TINY
	Name	= "SmartOS";
	Company	= "NewLife_Embedded_Team";

    Interrupt.Init();
#endif

	Started	= false;
}

#pragma arm section code

void ShowTime(void* param)
{
	debug_printf("\r");
	DateTime::Now().Show();
	debug_printf(" ");
}

void TSys::Init(void)
{
	InitClock();

	// 必须在系统主频率确定以后再初始化时钟
    ((TTime&)Time).Init();
}

void TSys::ShowInfo() const
{
#if DEBUG
	//byte* ver = (byte*)&Version;
	debug_printf("%s::%s Code:%04X ", Company, Name, Code);
	//debug_printf("Ver:%x.%x Build:%s\r\n", *ver++, *ver++, BuildTime);
	Version v(0, 0, Ver, 0);
	debug_printf("Ver:%s Build:%s\r\n", v.ToString().GetBuffer(), v.Compile().ToString().GetBuffer());

	OnShowInfo();

    debug_printf("ChipID:");
	ByteArray(ID, ArrayLength(ID)).Show();

	// 新的字符串这样用会导致第一个字符被清零
	//debug_printf("\t %s", ID);
	String str;
	str.Copy(0, ID, 12);
	str.Show(true);

	debug_printf("Time : ");
	DateTime::Now().Show(true);
	debug_printf("Support: http://www.WsLink.cn\r\n");

    debug_printf("\r\n");
#endif
}

#define __TASK__MODULE__ 1
#ifdef __TASK__MODULE__

// 任务
#include "Task.h"

// 创建任务，返回任务编号。dueTime首次调度时间ms，period调度间隔ms，-1表示仅处理一次
uint TSys::AddTask(Action func, void* param, int dueTime, int period, cstring name) const
{
	return Task::Scheduler()->Add(func, param, dueTime, period, name);
}

void TSys::RemoveTask(uint& taskid) const
{
	if(taskid) Task::Scheduler()->Remove(taskid);
	taskid = 0;
}

#if !defined(TINY) && defined(STM32F0) && defined(DEBUG)
	#pragma arm section code = "SectionForSys"
#endif

bool TSys::SetTask(uint taskid, bool enable, int msNextTime) const
{
	if(!taskid) return false;

	auto task = Task::Get(taskid);
	if(!task) return false;

	task->Set(enable, msNextTime);

	return true;
}

// 改变任务周期
bool TSys::SetTaskPeriod(uint taskid, int period) const
{
	if(!taskid) return false;

	auto task = Task::Get(taskid);
	if(!task) return false;

	if(period)
	{
		task->Period = period;

		// 改变任务周期的同时，重新计算下一次调度时间NextTime，让它立马生效
		// 否则有可能系统按照上一次计算好的NextTime再调度一次任务
		task->NextTime	= Ms() + period;
	}
	else
		task->Enable = false;

	return true;
}

void TSys::Start()
{
	OnStart();

	Started = true;

#if DEBUG
	//AddTask(ShowTime, nullptr, 2000000, 2000000);
#endif
	Task::Scheduler()->Start();
}

void TimeSleep(uint us)
{
	TS("Sys::TimeSleep");

	// 在这段时间里面，去处理一下别的任务
	if(Sys.Started && us != 0 && us >= 50)
	{
		auto sc	= Task::Scheduler();
		// 记录当前正在执行任务
		auto task = sc->Current;

		TimeCost tc;
		// 实际可用时间。100us一般不够调度新任务，留给硬件等待
		int total = us;
		/*int ts	= sc->Times;
		int tid	= 0;
		int tms	= 0;
		if(task)
		{
			tid	= task->ID;
			tms	= task->Times;
		}
		debug_printf("Sys::Sleep=> taskid=%d/%d us=%d \r\n", tid, tms, us);*/
		bool cancel	= false;
		// 如果休眠时间足够长，允许多次调度其它任务
		while(true)
		{
			// 统计这次调度的时间，累加作为当前任务的休眠时间
			TimeCost tc2;

			sc->Execute(total / 1000, cancel);

			total -= tc2.Elapsed();

			if(total <= 0) break;
		}

		int ct = tc.Elapsed();
		//debug_printf("Sys::Sleep<= taskid=%d/%d us=%d total=%d ct=%d Times=%d \r\n", tid, tms, us, total, ct, sc->Times - ts);
		if(task)
		{
			sc->Current = task;
			task->SleepTime += ct;
		}

		if(ct >= us) return;

		us -= ct;
	}
	if(us) Time.Delay(us);
}

// 系统启动后的毫秒数
UInt64 TSys::Ms() const { return Time.Current(); }
// 系统绝对当前时间，秒
uint TSys::Seconds() const { return Time.Seconds + Time.BaseSeconds; }

// 当前时间
DateTime DateTime::Now()
{
	DateTime dt(Sys.Seconds());
	dt.Ms = Sys.Ms();

	return dt;
}

void TSys::Sleep(uint ms) const
{
	// 优先使用线程级睡眠
	if(OnSleep)
		OnSleep(ms);
	else
	{
#if DEBUG
		if(ms > 1000) debug_printf("Sys::Sleep 设计错误，睡眠%dms太长！", ms);
#endif

		TimeSleep(ms * 1000);
	}
}

void TSys::Delay(uint us) const
{
	// 如果延迟微秒数太大，则使用线程级睡眠
	if(OnSleep && us >= 2000)
		OnSleep((us + 500) / 1000);
	else
	{
#if DEBUG
		if(us > 1000000) debug_printf("Sys::Sleep 设计错误，睡眠%dus太长！", us);
#endif

		if(us < 50)
			Time.Delay(us);
		else
			TimeSleep(us);
	}
}
#endif

#pragma arm section code

/****************系统跟踪****************/

//#if DEBUG
	#include "Port.h"
	static OutputPort* _trace = nullptr;
//#endif
void TSys::InitTrace(void* port) const
{
//#if DEBUG
	_trace	= (OutputPort*)port;
//#endif
}

void TSys::Trace(int times) const
{
//#if DEBUG
	if(_trace)
	{
		while(times--) *_trace = !*_trace;
	}
//#endif
}
