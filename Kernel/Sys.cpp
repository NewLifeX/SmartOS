#include "Sys.h"

#include "TTime.h"

TSys Sys;
const TTime Time;

#if defined(BOOT) || defined(APP)

struct HandlerRemap StrBoot __attribute__((at(0x2000fff0)));

#endif

#ifndef BIT
    #define BIT(x)	(1 << (x))
#endif

#if !defined(TINY) && defined(STM32F0)
	#if defined(__CC_ARM)
		#pragma arm section code = "SectionForSys"
	#elif defined(__GNUC__)
		__attribute__((section("SectionForSys")))
	#endif
#endif

TSys::TSys()
{
	Config	= nullptr;

	OnInit();

	OnSleep	= nullptr;

	Code	= 0x0000;
	Ver		= 0x17CC;
#ifndef TINY
	Name	= "SmartOS";
	Company	= "NewLife_Embedded_Team";

    Interrupt.Init();
#endif

	Started	= false;
}

#if !defined(TINY) && defined(STM32F0)
	#if defined(__CC_ARM)
		#pragma arm section code
	#elif defined(__GNUC__)
		__attribute__((section("")))
	#endif
#endif

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
	Version v(0, 0, Ver);
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

#if !defined(TINY) && defined(STM32F0)
	#if defined(__CC_ARM)
		#pragma arm section code = "SectionForSys"
	#elif defined(__GNUC__)
		__attribute__((section("SectionForSys")))
	#endif
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

	Task::Scheduler()->Start();
}

// 延迟异步重启
void TSys::Reboot(int msDelay) const
{
	if (msDelay <= 0)Reset();

	auto func	= &TSys::Reset;
	AddTask(*(Action*)&func, (void*)this, msDelay, -1, "延迟重启");
}

// 系统启动后的毫秒数
UInt64 TSys::Ms() const { return Time.Current(); }
// 系统绝对当前时间，秒
uint TSys::Seconds() const { return Time.Seconds + Time.BaseSeconds; }

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

		// 在这段时间里面，去处理一下别的任务
		if(Sys.Started && ms != 0)
		{
			bool cancel	= false;
			int ct	= Task::Scheduler()->ExecuteForWait(ms, cancel);

			if(ct >= ms) return;

			ms	-= ct;
		}
		if(ms) Time.Sleep(ms);
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

		// 在这段时间里面，去处理一下别的任务
		if(Sys.Started && us != 0 && us >= 50)
		{
			bool cancel	= false;
			auto ct	= Task::Scheduler()->ExecuteForWait(us / 1000, cancel);
			ct	*= 1000;

			if(ct >= us) return;

			us -= ct;
		}
		if(us) Time.Delay(us);
	}
}
#endif

#if !defined(TINY) && defined(STM32F0)
	#if defined(__CC_ARM)
		#pragma arm section code
	#elif defined(__GNUC__)
		__attribute__((section("")))
	#endif
#endif

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
