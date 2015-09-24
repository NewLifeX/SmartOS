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

	byte temp = ms.Read<byte>();
	Code	= temp & 0x3f;
	Reply	= temp >> 7;
	Error	= (temp >> 6) & 0x01;
	Length	= ms.Read<byte>();

	// 占位符拷贝到实际数据
	/*Code	= _Code;
	Length	= _Length;
	Reply	= _Reply;
	Error	= _Error;*/

	if(ms.Remain() < Length) return false;

	assert_param2(Data != _Data || Length <= ArrayLength(_Data), "令牌消息太大，缓冲区无法放下");
	if(Length > 0) ms.Read(Data, 0, Length);

	return true;
}

// 把消息写入到数据流中
void TokenMessage::Write(Stream& ms) const
{
	assert_ptr(this);

	// 实际数据拷贝到占位符
	/*TokenMessage* p = (TokenMessage*)this;
	p->_Code	= Code;
	p->_Reply	= Reply;
	p->_Error	= Error;
	p->_Length	= Length;

	byte tmp = _Code | (_Reply << 7) | (_Error << 6);
	ms.Write(tmp);
	ms.Write(_Length);*/

	byte tmp = Code | (Reply << 7) | (Error << 6);
	ms.Write(tmp);

	if(!Data)
		ms.Write((byte)0);
	else
	{
		ms.Write(Length);
		if(Length > 0) ms.Write(Data, 0, Length);
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
		else if(Reply)
			debug_printf(" Reply");
		debug_printf(" _Code=0x%02X", Code);
	}

	if(Length > 0)
	{
		assert_ptr(Data);
		debug_printf(" Data[%d]=", Length);
		//Sys.ShowHex(Data, Length, false);
		ByteArray bs(Data, Length);
		bs.Show();
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

	if(!Stat)
	{
		Stat = new TokenStat();
		//Stat->Start();
		//debug_printf("TokenStat::令牌统计 ");
#if DEBUG
		_taskID = Sys.AddTask(StatTask, this, 5000000, 30000000, "令牌统计");
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
bool TokenController::Send(byte code, byte* buf, uint len)
{
	TokenMessage msg;
	msg.Code = code;
	msg.SetData(buf, len);

	return Send(msg);
}

bool TokenController::Dispatch(Stream& ms, Message* pmsg)
{
	TokenMessage msg;
	return Controller::Dispatch(ms, &msg);
}

// 收到消息校验后调用该函数。返回值决定消息是否有效，无效消息不交给处理器处理
bool TokenController::Valid(const Message& msg)
{
	// 代码为0是非法的
	if(!msg.Code) return false;

#if MSG_DEBUG
	/*msg_printf("TokenController::Dispatch ");
	// 输出整条信息
	Sys.ShowHex(buf, ms.Length, '-');
	msg_printf("\r\n");*/
#endif

	return true;
}

bool Encrypt(Message& msg, const ByteArray& pass)
{
	// 加解密。握手不加密，握手响应不加密
	if(msg.Length > 0 && pass.Length() > 0 && !(msg.Code == 0x01 || msg.Code == 0x08))
	{
		ByteArray bs(msg.Data, msg.Length);
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
		_Response->SetData(msg.Data, msg.Length);
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
		if(!rs) Stat->ReceiveReply++;
	}
	else
	{
		Stat->Receive++;
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

	msg.Show();

	// 如果是错误，显示错误信息
	if(msg.Error)
	{
		debug_printf("Error=0x%02X ", msg.Data[0]);
		if(msg.Data[0] == 0x01)
		{
			Stream ms(msg.Data + 1, msg.Length - 1);
			ms.ReadString().Show(false);
		}
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

	Stat->Send++;

	for(int i=0; i<ArrayLength(_Queue); i++)
	{
		if(_Queue[i].Code == 0)
		{
			_Queue[i].Code	= code;
			_Queue[i].Time	= Time.Current();
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
				int cost = (int)(Time.Current() - _Queue[i].Time);
				// 莫名其妙，有时候竟然是负数
				if(cost < 0) cost = -cost;
				if(cost < 1000000)
				{
					Stat->Success++;
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
	Stat->ToStr(str.Clear());
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
	Send		= 0;
	Success		= 0;
	SendReply	= 0;
	Time		= 0;
	Receive		= 0;
	ReceiveReply= 0;

	_Last		= NULL;
	_Total		= NULL;
}

TokenStat::~TokenStat()
{
	if (_Last == NULL) delete _Last;
	if (_Total == NULL) delete _Total;
}

int TokenStat::Percent() const
{
	int send = Send;
	int sucs = Success;
	if(_Last)
	{
		send += _Last->Send;
		sucs += _Last->Success;
	}
	if(send == 0) return 0;

	return sucs * 10000 / send;
}

int TokenStat::Speed() const
{
	int time = Time;
	int sucs = Success;
	if(_Last)
	{
		time += _Last->Time;
		sucs += _Last->Success;
	}
	if(sucs == 0) return 0;

	return time / sucs;
}

void TokenStat::Clear()
{
	if (_Last == NULL) _Last = new TokenStat();
	if (_Total == NULL) _Total = new TokenStat();

	_Last->Send = Send;
	_Last->Success = Success;
	_Last->SendReply = SendReply;
	_Last->Time = Time;
	_Last->Receive = Receive;
	_Last->ReceiveReply = ReceiveReply;

	_Total->Send += Send;
	_Total->Success += Success;
	_Total->SendReply += SendReply;
	_Total->Time += Time;
	_Total->Receive += Receive;
	_Total->ReceiveReply += ReceiveReply;

	Send = 0;
	Success = 0;
	SendReply = 0;
	Time = 0;
	Receive = 0;
	ReceiveReply = 0;
}

String& TokenStat::ToStr(String& str) const
{
	int p = Percent();
	int q = p % 100;
	p /= 100;
	if(q == 0)
		str = str + "发：" + p + "%";
	else
		str = str + "发：" + p + "." + p + "%";
	str = str + " " + Success + "/" + Send + "/" + SendReply + " " + Speed() + "us 收：" + Receive + "/" + ReceiveReply;
	if(_Total)
	{
		str += "总";
		_Total->ToStr(str);
	}

	return str;
}
