#include "Time.h"
#include "TokenMessage.h"

#include "Net\Net.h"
#include "Security\RC4.h"

#define MSG_DEBUG DEBUG
//#define MSG_DEBUG 0

/******************************** TokenMessage ********************************/

// 使用指定功能码初始化令牌消息
TokenMessage::TokenMessage(byte code) : Message(code)
{
	Data	= _Data;

	/*_Code	= 0;
	_Reply	= 0;
	_Error	= 0;
	_Length	= 0;*/
}

// 从数据流中读取消息
bool TokenMessage::Read(Stream& ms)
{
	assert_ptr(this);
	if(ms.Remain() < MinSize) return false;

	byte temp = ms.ReadByte();
	Code	= temp & 0x3f;
	Reply	= temp >> 7;
	Error	= (temp >> 6) & 0x01;
	//Length	= ms.ReadByte();
	ushort len	= ms.ReadEncodeInt();
	Length	= len;

	if(ms.Remain() < len) return false;

	// 避免错误指令超长，导致溢出
	if(Data == _Data && len > ArrayLength(_Data))
	{
		debug_printf("错误指令，长度 %d 大于消息数据缓冲区长度 %d \r\n", len, ArrayLength(_Data));
		//assert_param(false);
		return false;
	}
	if(len > 0) ms.Read(Data, 0, len);

	return true;
}

// 把消息写入到数据流中
void TokenMessage::Write(Stream& ms) const
{
	assert_ptr(this);

	byte tmp = Code | (Reply << 7) | (Error << 6);
	ms.Write(tmp);

	if(!Data)
		ms.Write((byte)0);
	else
	{
		ushort len	= Length;
		ms.WriteEncodeInt(len);
		if(len > 0) ms.Write(Data, 0, len);
	}
}

// 验证消息校验码是否有效
bool TokenMessage::Valid() const
{
	return true;
}

// 消息总长度，包括头部、负载数据和校验
uint TokenMessage::Size() const
{
	return HeaderSize + Length;
}

uint TokenMessage::MaxDataSize() const
{
	return Data == _Data ? ArrayLength(_Data) : Length;
}

// 设置错误信息字符串
void TokenMessage::SetError(byte errorCode, string error, int errLength)
{
	Error = errorCode != 0;
	Length = 1 + errLength;
	Data[0] = errorCode;
	if(errLength > 0)
	{
		assert_ptr(error);
		memcpy(Data + 1, error, errLength);
	}
}

void TokenMessage::Show() const
{
#if MSG_DEBUG
	assert_ptr(this);

	byte code = Code;
	if(Reply) code |= 0x80;
	if(Error) code |= 0x40;

	debug_printf("Code=0x%02X", code);
	if(Reply || Error)
	{
		if(Error)
			debug_printf(" Error");
		//else if(Reply)
		//	debug_printf(" Reply");
		debug_printf(" _Code=0x%02X", Code);
	}

	ushort len	= Length;
	if(len > 0)
	{
		assert_ptr(Data);
		debug_printf(" Data[%d]=", len);
		// 大于32字节时，反正都要换行显示，干脆一开始就换行，让它对齐
		if(len > 32) debug_printf("\r\n");
		ByteArray(Data, len).Show();
	}
	debug_printf("\r\n");
#endif
}

/******************************** TokenController ********************************/

TokenController::TokenController() : Controller(), Key(0)
{
	Token	= 0;

	MinSize = TokenMessage::MinSize;
	//MaxSize = 1500;

	Server	= NULL;

	// 默认屏蔽心跳日志和确认日志
	ArrayZero(NoLogCodes);
	NoLogCodes[0] = 0x03;
	NoLogCodes[1] = 0x08;

	_Response = NULL;

	Stat	= NULL;

	memset(_Queue, 0, ArrayLength(_Queue) * sizeof(_Queue[0]));
}

TokenController::~TokenController()
{

}

void TokenController::Open()
{
	if(Opened) return;

	assert_param2(Port, "还没有传输口呢");

	debug_printf("TokenNet::Inited 使用传输接口 %s\r\n", Port->ToString());

	ISocket* sock = dynamic_cast<ISocket*>(Port);
	if(sock)
	{
		Server	= &sock->Remote;
	}

	if(!Stat)
	{
		Stat = new TokenStat();
		//Stat->Start();
		//debug_printf("TokenStat::令牌统计 ");
#if DEBUG
		_taskID = Sys.AddTask(StatTask, this, 5000, 30000, "令牌统计");
#endif
	}

	Controller::Open();
}

void TokenController::Close()
{
	delete Stat;
	Stat = NULL;

	Sys.RemoveTask(_taskID);
}

// 发送消息，传输口参数为空时向所有传输口发送消息
bool TokenController::Send(byte code, const Array& arr)
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
	// 代码为0是非法的
	if(!msg.Code) return false;

	// 握手和登录指令可直接通过
	if(msg.Code <= 0x02) return true;

	// 合法来源验证，暂时验证云平台，其它连接将来验证
	if(Server)
	{
		auto svr	= (IPEndPoint*)Server;
		auto rmt	= (IPEndPoint*)msg.State;

		if(!rmt || *svr != *rmt)
		{
			debug_printf("Token::Valid 非法来源 ");
			if(rmt) rmt->Show();
			debug_printf("\r\n");
			return false;
		}
	}

#if MSG_DEBUG
	/*msg_printf("TokenController::Dispatch ");
	// 输出整条信息
	ByteArray(buf, ms.Length).Show(true);*/
#endif

	return true;
}

static bool Encrypt(Message& msg, const Array& pass)
{
	// 加解密。握手不加密，握手响应不加密
	if(msg.Length > 0 && pass.Length() > 0 && !(msg.Code == 0x01 || msg.Code == 0x08))
	{
		Array bs(msg.Data, msg.Length);
		RC4::Encrypt(bs, pass);
		return true;
	}
	return false;
}

// 接收处理函数
bool TokenController::OnReceive(Message& msg)
{
	byte code = msg.Code;
	if(msg.Reply) code |= 0x80;
	if(msg.Error) code |= 0x40;

	// 起点和终点节点，收到响应时需要发出确认指令，而收到请求时不需要
	// 系统指令也不需要确认
	if(msg.Code >= 0x10 && msg.Code != 0x08)
	{
		// 需要为请求发出确认，因为转发以后不知道还要等多久才能收到另一方的响应
		//if(msg.Reply)
		{
			TokenMessage ack;
			ack.Code = 0x08;
			ack.Length = 1;
			ack.Data[0] = code;	// 这里考虑最高位

			Reply(ack);
		}
	}

	// 如果有等待响应，则交给它
	if(msg.Reply && _Response && (msg.Code == _Response->Code || msg.Code == 0x08 && msg.Data[0] == _Response->Code))
	{
		_Response->SetData(Array(msg.Data, msg.Length));
		_Response->Reply = true;

		// 加解密。握手不加密，登录响应不加密
		Encrypt(msg, Key);

		ShowMessage("RecvSync", msg);

		return true;
	}

	// 确认指令已完成使命直接跳出
	if(msg.Code == 0x08)
	{
		if(msg.Length >= 1) EndSendStat(msg.Data[0], true);

		ShowMessage("Recv", msg);

		return true;
	}

	if(msg.Reply)
	{
		bool rs = EndSendStat(code, true);

		// 如果匹配了发送队列，那么这里不再作为收到响应
		if(!rs) Stat->RecvReplyAsync++;
	}
	else
	{
		Stat->RecvRequest++;
	}

	//ShowMessage("Recv$", msg);

	// 加解密。握手不加密，登录响应不加密
	Encrypt(msg, Key);

	ShowMessage("Recv", msg);

	return Controller::OnReceive(msg);
}

// 发送消息，传输口参数为空时向所有传输口发送消息
bool TokenController::Send(Message& msg)
{
	TS("TokenController::Send");

	if(msg.Reply)
		ShowMessage("Reply", msg);
	else
		ShowMessage("Send", msg);

	// 加解密。握手不加密，登录响应不加密
	Encrypt(msg, Key);

	// 加入统计
	if(!msg.Reply) StartSendStat(msg.Code);

	return Controller::Send(msg);
}

// 发送消息并接受响应，msTimeout毫秒超时时间内，如果对方没有响应，会重复发送
bool TokenController::SendAndReceive(TokenMessage& msg, int retry, int msTimeout)
{
#if MSG_DEBUG
	if(_Response) debug_printf("设计错误！正在等待Code=0x%02X的消息，完成之前不能再次调用\r\n", _Response->Code);

	TimeCost ct;
#endif

	if(msg.Reply) return Send(msg) != 0;

	byte code = msg.Code;
	if(msg.Reply) code |= 0x80;
	if(msg.Error) code |= 0x40;
	// 加入统计
	if(!msg.Reply) StartSendStat(msg.Code);

	_Response = &msg;

	bool rs = false;
	while(retry-- >= 0)
	{
		if(!Send(msg)) break;

		// 等待响应
		TimeWheel tw(0, msTimeout);
		tw.Sleep = 1;
		do
		{
			if(_Response->Reply)
			{
				rs = true;
				break;
			}
		}while(!tw.Expired());
		if(rs) break;
	}

#if MSG_DEBUG
	debug_printf("Token::SendAndReceive Len=%d Time=%dus ", msg.Size(), ct.Elapsed());
	if(rs) _Response->Show();
	debug_printf("\r\n");
#endif

	_Response = NULL;

	EndSendStat(code, rs);

	return rs;
}

void TokenController::ShowMessage(string action, Message& msg)
{
#if MSG_DEBUG
	for(int i=0; i<ArrayLength(NoLogCodes); i++)
	{
		if(msg.Code == NoLogCodes[i]) return;
		if(NoLogCodes[i] == 0) break;
	}

	debug_printf("Token::%s ", action);

	if(msg.State)
	{
		auto svr	= (IPEndPoint*)Server;
		auto rmt	= (IPEndPoint*)msg.State;
		if(!svr || *svr != *rmt)
		{
			rmt->Show();
			debug_printf(" ");
		}
	}

	msg.Show();

	// 如果是错误，显示错误信息
	if(msg.Error)
	{
		debug_printf("Error=0x%02X ", msg.Data[0]);
		if(msg.Data[0] == 0x01 && msg.Length - 1 < 0x40)
		{
			Stream ms(msg.Data + 1, msg.Length - 1);
			ms.ReadString().Show(false);
		}
		debug_printf("\r\n");
	}
#endif
}

bool TokenController::StartSendStat(byte code)
{
	// 仅统计请求信息，不统计响应信息
	if ((code & 0x80) != 0)
	{
		Stat->SendReply++;
		return true;
	}

	Stat->SendRequest++;

	for(int i=0; i<ArrayLength(_Queue); i++)
	{
		if(_Queue[i].Code == 0)
		{
			_Queue[i].Code	= code;
			_Queue[i].Time	= Sys.Ms();
			return true;
		}
	}

	return false;
}

bool TokenController::EndSendStat(byte code, bool success)
{
	code &= 0x3F;

	for(int i=0; i<ArrayLength(_Queue); i++)
	{
		if(_Queue[i].Code == code)
		{
			bool rs = false;
			if(success)
			{
				int cost = (int)(Sys.Ms() - _Queue[i].Time);
				// 莫名其妙，有时候竟然是负数
				if(cost < 0) cost = -cost;
				if(cost < 1000)
				{
					Stat->RecvReply++;
					Stat->Time += cost;

					rs = true;
				}
			}

			_Queue[i].Code	= 0;

			return rs;
		}
	}

	return false;
}

void TokenController::ShowStat()
{
	char cs[128];
	String str(cs, ArrayLength(cs));
	str.Clear();
	Stat->ToStr(str);
	str.Show(true);

	Stat->Clear();

	// 向以太网广播
	ISocket* sock = dynamic_cast<ISocket*>(Port);
	if(sock)
	{
		ByteArray bs(str);
		IPEndPoint ep(IPAddress::Broadcast(), 514);
		sock->SendTo(bs, ep);
	}
}

void TokenController::StatTask(void* param)
{
	TokenController* st = (TokenController*)param;
	st->ShowStat();
}

/******************************** TokenStat ********************************/

TokenStat::TokenStat()
{
	/*SendRequest	= 0;
	RecvReply	= 0;
	SendReply	= 0;
	Time		= 0;
	RecvRequest	= 0;
	RecvReplyAsync	= 0;

	Read		= 0;

	_Last		= NULL;
	_Total		= NULL;*/

	int start	= offsetof(TokenStat, SendRequest);
	memset((byte*)this + start, 0, sizeof(TokenStat) - start);
}

TokenStat::~TokenStat()
{
	if (_Last	== NULL)	delete _Last;
	if (_Total	== NULL)	delete _Total;
}

int TokenStat::Percent() const
{
	int send = SendRequest;
	int sucs = RecvReply;
	if(_Last)
	{
		send += _Last->SendRequest;
		sucs += _Last->RecvReply;
	}
	if(send == 0) return 0;

	return sucs * 100 / send;
}

int TokenStat::Speed() const
{
	int time = Time;
	int sucs = RecvReply;
	if(_Last)
	{
		time += _Last->Time;
		sucs += _Last->RecvReply;
	}
	if(sucs == 0) return 0;

	return time / sucs;
}

int TokenStat::PercentReply() const
{
	int req = RecvRequest;
	int rep = SendReply;
	if(_Last)
	{
		req += _Last->RecvRequest;
		rep += _Last->SendReply;
	}
	if(req == 0) return 0;

	return rep * 100 / req;
}

void TokenStat::Clear()
{
	if (_Last == NULL) _Last = new TokenStat();
	if (_Total == NULL) _Total = new TokenStat();

	_Last->SendRequest	= SendRequest;
	_Last->RecvReply	= RecvReply;
	_Last->SendReply	= SendReply;
	_Last->Time = Time;
	_Last->RecvRequest	= RecvRequest;
	_Last->RecvReplyAsync	= RecvReplyAsync;
	_Last->Read			= Read;
	_Last->ReadReply	= ReadReply;
	_Last->Write		= Write;
	_Last->WriteReply	= WriteReply;

	_Total->SendRequest	+= SendRequest;
	_Total->RecvReply	+= RecvReply;
	_Total->SendReply	+= SendReply;
	_Total->Time += Time;
	_Total->RecvRequest	+= RecvRequest;
	_Total->RecvReplyAsync	+= RecvReplyAsync;
	_Total->Read		+= Read;
	_Total->ReadReply	+= ReadReply;
	_Total->Write		+= Write;
	_Total->WriteReply	+= WriteReply;

	SendRequest	= 0;
	RecvReply	= 0;
	Time		= 0;

	SendReply	= 0;
	RecvRequest	= 0;
	RecvReplyAsync	= 0;

	Read		= 0;
	ReadReply	= 0;
	Write		= 0;
	WriteReply	= 0;
}

String& TokenStat::ToStr(String& str) const
{
	str = str + "发：" + Percent() + "% " + RecvReply + "/" + SendRequest + " " + Speed() + "ms";
	str = str + " 收：" + PercentReply() + "% " + SendReply + "/" + RecvRequest + " 异步" + RecvReplyAsync;
	if (Read > 0) str = str + " 读：" + (ReadReply * 100 / Read) + " " + ReadReply + "/" + Read;
	if (Write > 0) str = str + " 写：" + (WriteReply * 100 / Write) + " " + WriteReply + "/" + Write;
	if(_Total)
	{
		str += "总";
		_Total->ToStr(str);
	}

	return str;
}
