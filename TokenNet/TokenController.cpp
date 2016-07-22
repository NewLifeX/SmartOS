#include "TokenController.h"

#include "Net\Socket.h"
#include "Security\RC4.h"
#include "Security\Crc.h"

#define MSG_DEBUG DEBUG
//#define MSG_DEBUG 0

// 令牌统计
class TokenStat : public Object
{
public:
	// 发送统计
	int	SendRequest;
	int	RecvReply;
	int	Time;

	String Percent() const;	// 成功率百分比，已乘以100
	int Speed() const;		// 平均速度，指令发出到收到响应的时间

	// 接收统计
	int	RecvRequest;
	int	SendReply;
	int	RecvReplyAsync;
	String PercentReply() const;

	// 数据操作统计
	int Read;
	int ReadReply;
	int Write;
	int WriteReply;
	int Invoke;
	int InvokeReply;

	TokenStat();
	~TokenStat();

	void Clear();

	virtual String& ToStr(String& str) const;

private:
	TokenStat*	_Last;
	TokenStat*	_Total;
};

#if DEBUG
// 全局的令牌统计指针
static TokenStat* Stat = nullptr;

static void StatTask(void* param);
#endif

/******************************** TokenController ********************************/

TokenController::TokenController() : Controller(), Key(0)
{
	Token = 0;

	MinSize = TokenMessage::MinSize;

	Socket = nullptr;
	Server = nullptr;

	ShowRemote = false;

	// 默认屏蔽心跳日志和确认日志
	Buffer(NoLogCodes, sizeof(NoLogCodes)).Clear();
	NoLogCodes[0] = 0x03;

	Buffer(_RecvQueue, sizeof(_RecvQueue)).Clear();
}

TokenController::~TokenController()
{

}

void TokenController::Open()
{
	if (Opened) return;

	assert(Socket, "还没有Socket呢");
	if (!Port) Port = dynamic_cast<ITransport*>(Socket);
	assert(Port, "还没有传输口呢");

	debug_printf("TokenNet::Inited 使用传输接口 ");
#if DEBUG
	auto obj = dynamic_cast<Object*>(Port);
	if (obj) obj->Show(true);
	//Port->Show(true);
#endif

	Server = &Socket->Remote;

#if DEBUG
	if (!Stat)
	{
		Stat = new TokenStat();
		Sys.AddTask(StatTask, Stat, 5000, 30000, "令牌统计");
	}
#endif

	Controller::Open();
}

void TokenController::Close()
{
}

// 发送消息，传输口参数为空时向所有传输口发送消息
bool TokenController::Send(byte code, const Buffer& arr)
{
	TokenMessage msg;
	msg.Code = code;
	msg.SetData(arr);

	return Send(msg);
}

bool TokenController::Dispatch(Stream& ms, Message* pmsg, void* param)
{
	TokenMessage msg;
	return Controller::Dispatch(ms, &msg, param);
}

// 收到消息校验后调用该函数。返回值决定消息是否有效，无效消息不交给处理器处理
bool TokenController::Valid(const Message& msg)
{
	TS("TokenController::Valid");

	// 代码为0是非法的
	if (!msg.Code) return false;

	// 握手和登录指令可直接通过
	if (msg.Code <= 0x02) return true;

	if (Token != 0) return true;

	// 合法来源验证，暂时验证云平台，其它连接将来验证
	//auto svr = (IPEndPoint*)Server;
	auto rmt = (IPEndPoint*)msg.State;

	if (!rmt)
	{
		debug_printf("Token::Valid 非法来源 ");
		if (rmt) rmt->Show();
		debug_printf("\r\n");
		return false;
	}

#if MSG_DEBUG
	/*msg_printf("TokenController::Dispatch ");
	// 输出整条信息
	ByteArray(buf, ms.Length).Show(true);*/
#endif

	return true;
}

// msg所有成员序列化为data
static bool Encrypt(Stream& ms, const Buffer& pass)
{
	if (ms.Length <= 3) return false;
	if (pass.Length() == 0) return true;

	ms.Seek(2);

	auto len = ms.ReadEncodeInt();
	Buffer bs(ms.ReadBytes(len), len);

	// 计算明文校验码，写在最后面
	auto crc = Crc::Hash16(bs);
	RC4::Encrypt(bs, pass);

	ms.Write(crc);

	return true;
}

// Decrypt(Buffer(msg.Data,len),Key)  只处理data部分
static bool Decrypt(Buffer& data, const Buffer& pass)
{
	if (data.Length() <= 3) return false;
	if (pass.Length() == 0) return true;

	auto msgDataLen = data.Length() - 2;
	Stream ms(data);
	ms.Seek(msgDataLen);
	auto crc = ms.ReadUInt16();		// 读取数据包中crc

	data.SetLength(msgDataLen);
	ByteArray bs(data.GetBuffer(), data.Length());
	RC4::Encrypt(bs, pass);			// 解密

	auto crc2 = Crc::Hash16(bs);	// 明文的CRC值
	if (crc != crc2) return false;

	return true;
}

// 接收处理函数
bool TokenController::OnReceive(Message& msg)
{
	TS("TokenController::OnReceive");

	if (msg.Reply)
	{
	}
	else
	{
		auto& tmsg = (TokenMessage&)msg;
		// 过滤重复请求。1秒内不接收重复指令
		UInt64 start = Sys.Ms() - 5000;
		for (int i = 0; i < ArrayLength(_RecvQueue); i++)
		{
			auto& qi = _RecvQueue[i];
			if (qi.Code == msg.Code && qi.Seq == tmsg.Seq && qi.Time > start) return true;
		}
		for (int i = 0; i < ArrayLength(_RecvQueue); i++)
		{
			auto& qi = _RecvQueue[i];
			if (qi.Code == 0 || qi.Time <= start)
			{
				qi.Code = msg.Code;
				qi.Seq = tmsg.Seq;
				qi.Time = Sys.Ms();

				break;
			}
		}
	}
#if DEBUG
	bool rs = StatReceive(msg);
#endif

	// 加解密。握手不加密，登录响应不加密
	if (msg.Code > 0x01)
	{
		Buffer bs(msg.Data, msg.Length + 2);
		if (!Decrypt(bs, Key))
		{
			debug_printf("TokenController::OnReceive 解密失败 Key:\r\n");
			auto remote = (IPEndPoint*)msg.State;
			remote->Show(false);
			debug_printf("  Code 0x%02X Key: ", msg.Code);
			Key.Show(true);

			msg.Length = 0;
			return Controller::OnReceive(msg);
		}
	}

	ShowMessage("Recv", msg);

	return Controller::OnReceive(msg);
}

static byte _Sequence = 0;
// 发送消息，传输口参数为空时向所有传输口发送消息
bool TokenController::Send(Message& msg)
{
	TS("TokenController::Send");

	//static byte _Sequence	= 0;
	auto& tmsg = (TokenMessage&)msg;
	// 附上序列号。响应消息保持序列号不变
	if (!msg.Reply && tmsg.Seq == 0) tmsg.Seq = ++_Sequence;

	if (msg.Reply)
		ShowMessage("Reply", msg);
	else
		ShowMessage("Send", msg);

	// 如果没有传输口处于打开状态，则发送失败
	if (!Port->Open()) return false;

	//byte buf[1472];
	//Stream ms(buf, ArrayLength(buf));
	byte buf[256];
	MemoryStream ms(buf, ArrayLength(buf));
	// 带有负载数据，需要合并成为一段连续的内存
	msg.Write(ms);

	// 握手不加密
	if (msg.Code > 0x01 && Key.Length() > 0)
	{
		ms.SetPosition(0);
		if (!Encrypt(ms, Key)) return false;
	}

#if DEBUG
	// 加入统计
	StatSend(msg);
#endif

	Buffer bs(ms.GetBuffer(), ms.Position());

	return Controller::SendInternal(bs, msg.State);
}

void TokenController::ShowMessage(cstring action, const Message& msg)
{
#if MSG_DEBUG
	TS("TokenController::ShowMessage");

	for (int i = 0; i < ArrayLength(NoLogCodes); i++)
	{
		if (msg.Code == NoLogCodes[i]) return;
		if (NoLogCodes[i] == 0) break;
	}

	debug_printf("Token::%s ", action);

	if (ShowRemote && msg.State)
	{
		auto svr = (IPEndPoint*)msg.State;
		svr->Show();
		debug_printf(" ");
	}

	msg.Show();

	// 如果是错误，显示错误信息
	if (msg.Error)
	{
		debug_printf("Error=0x%02X ", msg.Data[0]);
		if (msg.Data[0] == 0x01 && msg.Length - 1 < 0x40)
		{
			Stream ms(msg.Data + 1, msg.Length - 1);
			ms.ReadString().Show(false);
		}
		debug_printf("\r\n");
	}
#endif
}

#if DEBUG
void StatRequest(byte code)
{
	auto st = Stat;

	byte tc = code & 0x0F;
	if (tc == 0x05)
		st->Read++;
	else if (tc == 0x06)
		st->Write++;
	else if (tc == 0x08)
		st->Invoke++;
}

void StatReply(byte code)
{
	auto st = Stat;

	byte tc = code & 0x0F;
	if (tc == 0x05)
		st->ReadReply++;
	else if (tc == 0x06)
		st->WriteReply++;
	else if (tc == 0x08)
		st->InvokeReply++;
}

bool TokenController::StatSend(const Message& msg)
{
	byte code = msg.Code;
	auto st = Stat;

	// 统计请求
	if (!msg.Reply && !msg.OneWay)
	{
		st->SendRequest++;
		StatRequest(code);

		// 超时指令也干掉
		UInt64 end = Sys.Ms() - 5000;
		for (int i = 0; i < ArrayLength(_StatQueue); i++)
		{
			auto& qi = _StatQueue[i];
			if (qi.Code == 0 || qi.Time <= end)
			{
				qi.Code = code;
				qi.Time = Sys.Ms();

				return true;
			}
		}
	}

	// 统计响应
	if (msg.Reply)
	{
		st->SendReply++;
		StatReply(msg.Code);
	}

	return false;
}

bool TokenController::StatReceive(const Message& msg)
{
	byte code = msg.Code;
	auto st = Stat;

	if (msg.Reply)
	{
		bool rs = false;
		for (int i = 0; i < ArrayLength(_StatQueue); i++)
		{
			auto& qi = _StatQueue[i];
			if (qi.Code == code)
			{
				int cost = (int)(Sys.Ms() - (UInt64)qi.Time);
				// 莫名其妙，有时候竟然是负数
				if (cost < 0) debug_printf("cost=%d qi.Time=%d now=%d \r\n", cost, (int)qi.Time, (int)qi.Time + cost);
				if (cost < 0) cost = -cost;
				if (cost < 1000)
				{
					st->RecvReply++;
					st->Time += cost;

					rs = true;
				}

				qi.Code = 0;

				break;
			}
		}
		// 如果匹配了发送队列，那么这里不再作为收到响应
		if (!rs) st->RecvReplyAsync++;

		StatReply(code);

		return rs;
	}
	else
	{
		st->RecvRequest++;
		StatRequest(code);
	}

	return true;
}
#endif

/******************************** TokenStat ********************************/

TokenStat::TokenStat()
{
	//Buffer(_StatQueue, sizeof(_StatQueue)).Clear();
	Buffer(&SendRequest, (byte*)&_Total + sizeof(_Total) - (byte*)&SendRequest).Clear();
}

TokenStat::~TokenStat()
{
	if (_Last == nullptr)	delete _Last;
	if (_Total == nullptr)	delete _Total;
}

String CaclPercent(int d1, int d2)
{
	String str;
	if (d2 == 0) return str + "0";

	// 分开处理整数部分和小数部分
	d1 *= 100;
	int d = d1 / d2;
	//d1	%= d2;
	// %会产生乘减指令MLS，再算一次除法
	d1 -= d * d2;
	d1 *= 100;
	int f = d1 / d2;

	str += d;
	if (f > 0)
	{
		str += ".";
		if (f < 10) str += "0";
		str += f;
	}

	return str;
}

String TokenStat::Percent() const
{
	int send = SendRequest;
	int sucs = RecvReply + RecvReplyAsync;
	if (_Last)
	{
		send += _Last->SendRequest;
		sucs += _Last->RecvReply + _Last->RecvReplyAsync;
	}

	return CaclPercent(sucs, send);
}

int TokenStat::Speed() const
{
	int time = Time;
	int sucs = RecvReply;
	if (_Last)
	{
		time += _Last->Time;
		sucs += _Last->RecvReply;
	}
	if (sucs == 0) return 0;

	return time / sucs;
}

String TokenStat::PercentReply() const
{
	int req = RecvRequest;
	int rep = SendReply;
	if (_Last)
	{
		req += _Last->RecvRequest;
		rep += _Last->SendReply;
	}

	return CaclPercent(rep, req);
}

void TokenStat::Clear()
{
	if (_Last == nullptr) _Last = new TokenStat();
	if (_Total == nullptr) _Total = new TokenStat();

	_Last->SendRequest = SendRequest;
	_Last->RecvReply = RecvReply;
	_Last->SendReply = SendReply;
	_Last->Time = Time;
	_Last->RecvRequest = RecvRequest;
	_Last->RecvReplyAsync = RecvReplyAsync;
	_Last->Read = Read;
	_Last->ReadReply = ReadReply;
	_Last->Write = Write;
	_Last->WriteReply = WriteReply;
	_Last->Invoke = Invoke;
	_Last->InvokeReply = InvokeReply;

	_Total->SendRequest += SendRequest;
	_Total->RecvReply += RecvReply;
	_Total->SendReply += SendReply;
	_Total->Time += Time;
	_Total->RecvRequest += RecvRequest;
	_Total->RecvReplyAsync += RecvReplyAsync;
	_Total->Read += Read;
	_Total->ReadReply += ReadReply;
	_Total->Write += Write;
	_Total->WriteReply += WriteReply;
	_Total->Invoke += Invoke;
	_Total->InvokeReply += InvokeReply;

	SendRequest = 0;
	RecvReply = 0;
	Time = 0;

	SendReply = 0;
	RecvRequest = 0;
	RecvReplyAsync = 0;

	Read = 0;
	ReadReply = 0;
	Write = 0;
	WriteReply = 0;
	Invoke = 0;
	InvokeReply = 0;
}

String& TokenStat::ToStr(String& str) const
{
	TS("TokenStat::ToStr");

	/*debug_printf("this=0x%08X _Last=0x%08X _Total=0x%08X ", this, _Last, _Total);
	Percent().Show(true);*/
	if (SendRequest > 0)
	{
		str = str + "发：" + Percent() + "% " + RecvReply;
		if (RecvReplyAsync > 0) str = str + "+" + RecvReplyAsync;
		str = str + "/" + SendRequest + " " + Speed() + "ms ";
	}
	if (RecvRequest > 0)	str = str + "收：" + PercentReply() + "% " + SendReply + "/" + RecvRequest;
	if (Read > 0)	str = str + " 读：" + (ReadReply * 100 / Read) + "% " + ReadReply + "/" + Read;
	if (Write > 0)	str = str + " 写：" + (WriteReply * 100 / Write) + "% " + WriteReply + "/" + Write;
	if (Invoke > 0)	str = str + " 调：" + (InvokeReply * 100 / Invoke) + " " + InvokeReply + "/" + Invoke;
	if (_Total)
	{
		str += " 总";
		_Total->ToStr(str);
	}

	return str;
}

#if DEBUG
void StatTask(void* param)
{
	TS("TokenController::ShowStat");

	auto st = (TokenStat*)param;
	// 这里输出  字节数 在 90-140   为了减少 new 直接使用256
	char cs[256];
	String str(cs, ArrayLength(cs));
	str.SetLength(0);
	st->ToStr(str);
	str.Show(true);

	st->Clear();

	// 向以太网广播
	/*auto sock = dynamic_cast<ISocket*>(Port);
	if(sock)
	{
		IPEndPoint ep(IPAddress::Broadcast(), 514);
		sock->SendTo(str, ep);
	}*/
}
#endif
