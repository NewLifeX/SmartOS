#include "Kernel\Sys.h"
#include "Kernel\Task.h"
#include "Kernel\TTime.h"
#include "Kernel\WaitHandle.h"

#include "Device\SerialPort.h"

#include "AT.h"

#include "Config.h"

#include "App\FlushPort.h"

#define NET_DEBUG DEBUG
//#define NET_DEBUG 0
#if NET_DEBUG
#define net_printf debug_printf
#else
#define net_printf(format, ...)
#endif

struct CmdState
{
	const String* Command = nullptr;
	String* Result = nullptr;
	cstring	Key1 = nullptr;
	cstring	Key2 = nullptr;
	cstring	Key3 = nullptr;

	bool	Capture = true;	// 是否捕获所有

	uint Parse(const Buffer& bs, WaitHandle& handle);
	uint FindKey(const String& str);
};

void LoadStationTask(void* param);

/*
		注意事项
1、设置模式AT+CWMODE需要重启后生效AT+RST
2、AP模式下查询本机IP无效，可能会造成死机
3、开启server需要多连接作为基础AT+CIPMUX=1
4、单连接模式，多连接模式  收发数据有参数个数区别
*/

/******************************** AT ********************************/

AT::AT()
{
	Port = nullptr;
	DataKey = nullptr;
	_Expect = nullptr;
}

AT::~AT()
{
	delete Port;
}

void AT::Init(COM idx, int baudrate)
{
	auto srp = new SerialPort(idx, baudrate);
	srp->Tx.SetCapacity(0x200);
	srp->Rx.SetCapacity(0x200);
	srp->MaxSize = 512;

	Init(srp);
}

void AT::Init(ITransport* port)
{
	Port = port;
	if (Port) Port->Register(OnPortReceive, this);
}

bool AT::Open()
{
	if (!Port->Open()) return false;

	/*if (!CheckReady())
	{
		net_printf("AT::Open 打开失败！");

		return false;
	}*/

	return true;
}

void AT::Close()
{
	Port->Close();
}

// 发送指令，在超时时间内等待返回期望字符串，然后返回内容
String AT::Send(const String& cmd, cstring expect, cstring expect2, uint msTimeout)
{
	TS("AT::Send");

	String rs;

	auto& task = Task::Current();
	// 判断是否正在发送其它指令
	if (_Expect)
	{
#if NET_DEBUG
		auto h = (WaitHandle*)_Expect;
		auto w = (CmdState*)h->State;
		net_printf("AT::Send %d 正在发送 ", h->TaskID);
		if (w->Command)
			w->Command->Trim().Show(false);
		else
			net_printf("数据");
		net_printf(" ，%d 无法发送 ", task.ID);
		cmd.Trim().Show(true);
#endif

		return rs;
	}

	// 在接收事件中拦截
	CmdState we;
	// 数据不显示Command，没有打开NET_DEBUG时也不显示
	//we.Command	= &cmd;
	we.Result = &rs;
	we.Key1 = expect;
	we.Key2 = expect2;
	we.Key3 = "busy ";

	WaitHandle handle;
	handle.State = &we;

	_Expect = &handle;

#if NET_DEBUG
	bool EnableLog = true;
#endif

	if (cmd)
	{
		Port->Write(cmd);

#if NET_DEBUG
		// 只有AT指令显示日志
		if (!cmd.StartsWith("AT") || (expect && expect[0] == '>')) EnableLog = false;
		if (EnableLog)
		{
			we.Command = &cmd;
			//net_printf("%d=> ", task.ID);
			cmd.Trim().Show(true);
		}
#endif
	}

	handle.WaitOne(msTimeout);
	if (_Expect == &handle) _Expect = nullptr;

	//if(rs.Length() > 4) rs.Trim();

#if NET_DEBUG
	if (EnableLog && rs)
	{
		net_printf("%d<= ", task.ID);
		// 太长时不要去尾，避免产生重新分配
		if (rs.Length() < 0x40)
			rs.Trim().Show(true);
		else
			rs.Show(true);
	}
#endif

	return rs;
}

// 发送命令，自动检测并加上\r\n，等待响应OK
bool AT::SendCmd(const String& cmd, uint msTimeout)
{
	TS("AT::SendCmd");

	static const cstring ok = "OK";
	static const cstring err = "ERROR";

	String cmd2;

	// 只有AT指令需要检查结尾，其它指令不检查，避免产生拷贝
	auto p = &cmd;
	if (cmd.StartsWith("AT") && !cmd.EndsWith("\r\n"))
	{
		cmd2 = cmd;
		cmd2 += "\r\n";
		p = &cmd2;
	}

	// 二级拦截。遇到错误也马上结束
	auto rt = Send(*p, ok, err, msTimeout);

	return rt.Contains(ok);
}

bool AT::WaitForCmd(cstring expect, uint msTimeout)
{
	String rs;

	// 在接收事件中拦截
	CmdState we;
	we.Result = &rs;
	we.Key1 = expect;
	we.Capture = false;

	WaitHandle handle;
	handle.State = &we;

	_Expect = &handle;

	// 等待收到数据
	bool rt = handle.WaitOne(msTimeout);

	_Expect = nullptr;

	return rt;
}

void ParseFail(cstring name, const Buffer& bs)
{
#if NET_DEBUG
	if (bs.Length() == 0) return;

	int p = 0;
	if (p < bs.Length() && bs[p] == ' ') p++;
	if (p < bs.Length() && bs[p] == '\r') p++;
	if (p < bs.Length() && bs[p] == '\n') p++;

	// 无法识别的数据可能是空格前缀，需要特殊处理
	auto str = bs.Sub(p, -1).AsString();
	if (str)
	{
		net_printf("AT:%s 无法识别[%d]：", name, bs.Length());
		//if(bs.Length() == 2) net_printf("%02X %02X ", bs[0], bs[1]);
		str.Show(true);
	}
#endif
}

uint AT::OnPortReceive(ITransport* sender, Buffer& bs, void* param, void* param2)
{
	auto esp = (AT*)param;
	return esp->OnReceive(bs, param2);
}

// 引发数据到达事件
uint AT::OnReceive(Buffer& bs, void* param)
{
	if (bs.Length() == 0) return 0;

	//!!! 分析数据和命令返回，特别要注意粘包
	int s = 0;
	int p = 0;
	auto str = bs.AsString();
	while (p >= 0 && p < bs.Length())
	{
		s = p;
		p = str.IndexOf(DataKey, s);

		// +IPD之前之后的数据，留给命令分析
		int size = p >= 0 ? p - s : bs.Length() - s;
		if (size > 0)
		{
			if (_Expect)
			{
				ParseReply(bs.Sub(s, size));
#if NET_DEBUG
				// 如果没有吃完，剩下部分报未识别
				//if(rs < size) ParseFail("ParseReply", bs.Sub(s + rs, size - rs));
				// 不要报未识别了，反正内部会全部吃掉
#endif
			}
			else
			{
#if NET_DEBUG
				ParseFail("NoExpect", bs.Sub(s, size));
#endif
			}
		}

		// +IPD开头的数据，作为收到数据
		if (p >= 0)
		{
			if (p + 5 >= bs.Length())
			{
#if NET_DEBUG
				ParseFail("+IPD<=5", bs.Sub(p, -1));
#endif
				break;
			}
			else
			{
				auto bs2 = bs.Sub(p, -1);
				Received(bs2);
				int rs = bs2.Length();
				if (rs <= 0)
				{
#if NET_DEBUG
					ParseFail("ParseReceive", bs.Sub(p + rs, -1));
#endif
					break;
				}

				// 游标移到下一组数据
				p += rs;
			}
		}
	}

	return 0;
}

// 分析关键字。返回被用掉的字节数
uint AT::ParseReply(const Buffer& bs)
{
	if (!_Expect) return 0;

	// 拦截给同步方法
	auto handle = (WaitHandle*)_Expect;
	auto we = (CmdState*)handle->State;
	bool rs = we->Parse(bs, *handle);

	// 如果内部已经适配，则清空
	if (!we->Result) _Expect = nullptr;

	return rs;
}

uint CmdState::Parse(const Buffer& bs, WaitHandle& handle)
{
	if (bs.Length() == 0 || !Result) return 0;

	TS("WaitExpect::Parse");

	// 适配任意关键字后，也就是收到了成功或失败，通知业务层已结束
	auto s_ = bs.AsString();
	auto& s = (const String&)s_;
	int p = FindKey(s);
	auto& rs = *Result;

	// 捕获所有
	if (Capture)
	{
		if (p > 0)
			rs += bs.Sub(0, p).AsString();
		else
			rs += s;
	}
	else if (p > 0)
		rs = bs.Sub(0, p).AsString();

	// 匹配关键字，任务完成
	if (p > 0)
	{
		Result = nullptr;

		// 设置事件，通知等待任务退出循环
		handle.Set();
	}

	// 如果后面是换行，则跳过
	if (p < s.Length() && s[p] == ' ') p++;
	if (p < s.Length() && s[p] == '\r') p++;
	if (p < s.Length() && s[p] == '\n') p++;

	return p;
}

uint CmdState::FindKey(const String& str)
{
	// 适配第一关键字
	int p = Key1 ? str.IndexOf(Key1) : -1;
	if (p >= 0)
	{
		//net_printf("适配第一关键字 %s \r\n", Key1);
		return p + String(Key1).Length();
	}
	// 适配第二关键字
	p = Key2 ? str.IndexOf(Key2) : -1;
	if (p >= 0)
	{
		net_printf("适配第二关键字 %s \r\n", Key2);
		return p + String(Key2).Length();
	}
	// 适配busy
	p = str.IndexOf(Key3);
	if (p >= 0)
	{
		net_printf("适配第三关键字 %s \r\n", Key3);
		return p + String(Key3).Length();
	}
	return 0;
}
