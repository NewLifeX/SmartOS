#include "Kernel\Sys.h"

#include "WaitExpect.h"

#define NET_DEBUG DEBUG
//#define NET_DEBUG 0
#if NET_DEBUG
	#define net_printf debug_printf
#else
	#define net_printf(format, ...)
#endif

/******************************** WaitExpect ********************************/

bool WaitExpect::Wait(int msTimeout)
{
	// 提前等待一会，再开始轮询，专门为了加快命中快速响应的指令
	Sys.Sleep(40);
	if(!Result) return true;

	/*// 等待收到数据
	TimeWheel tw(msTimeout - 40);
	// 默认检查间隔200ms，如果超时时间大于1000ms，则以四分之一为检查间隔
	// ESP8266串口任务平均时间为150ms左右，为了避免接收指令任务里面发送指令时等不到OK，需要加大检查间隔
	tw.Sleep	= 200;
	if(msTimeout > 1000) tw.Sleep	= msTimeout >> 2;
	if(tw.Sleep > 1000)	tw.Sleep	= 1000;

	while(Result)
	{
		if(tw.Expired()) return false;
	}

	return true;*/

	return Handle.WaitOne(msTimeout);
}

uint WaitExpect::Parse(const Buffer& bs)
{
	if(bs.Length() == 0 || !Result) return 0;

	TS("WaitExpect::Parse");

	// 适配任意关键字后，也就是收到了成功或失败，通知业务层已结束
	auto s_	= bs.AsString();
	auto& s	= (const String&)s_;
	int p	= FindKey(s);
	auto& rs= *Result;

	// 捕获所有
	if(Capture)
	{
		if(p > 0)
			rs	+= bs.Sub(0, p).AsString();
		else
			rs	+= s;
	}
	else if(p > 0)
		rs	= bs.Sub(0, p).AsString();

	// 匹配关键字，任务完成
	if(p > 0)
	{
		Result	= nullptr;

		// 设置事件，通知等待任务退出循环
		Handle.Set();
	}

	// 如果后面是换行，则跳过
	if(p < s.Length() && s[p] == ' ') p++;
	if(p < s.Length() && s[p] == '\r') p++;
	if(p < s.Length() && s[p] == '\n') p++;

	return p;
}

uint WaitExpect::FindKey(const String& str)
{
	// 适配第一关键字
	int p	= Key1 ? str.IndexOf(Key1) : -1;
	if(p >= 0)
	{
		//net_printf("适配第一关键字 %s \r\n", Key1);
		return p + String(Key1).Length();
	}
	// 适配第二关键字
	p	= Key2 ? str.IndexOf(Key2) : -1;
	if(p >= 0)
	{
		net_printf("适配第二关键字 %s \r\n", Key2);
		return p + String(Key2).Length();
	}
	// 适配busy
	p	= str.IndexOf("busy ");
	if(p >= 0)
	{
		net_printf("适配 busy  \r\n");
		return p + 4 + 1 + 4;
	}
	return 0;
}
