#include "Time.h"
#include "Message\DataStore.h"

#include "Esp8266.h"

#define NET_DEBUG DEBUG
//#define NET_DEBUG 0
#if NET_DEBUG
	#define net_printf debug_printf
#else
	#define net_printf(format, ...)
#endif

/*
		注意事项
1、设置模式AT+CWMODE需要重启后生效AT+RST
2、AP模式下查询本机IP无效，可能会造成死机
3、开启server需要多连接作为基础AT+CIPMUX=1
4、单连接模式，多连接模式  收发数据有参数个数区别
*/

/******************************** 内部Tcp/Udp ********************************/
class EspSocket : public Object, public ITransport, public ISocket
{
protected:
	Esp8266&	_Host;

public:
	EspSocket(Esp8266& host, ProtocolType protocol);
	virtual ~EspSocket();

	// 打开Socket
	virtual bool OnOpen();
	virtual void OnClose();

	//// 应用配置，修改远程地址和端口
	//virtual bool Change(const String& remote, ushort port);

	virtual bool OnWrite(const Buffer& bs);
	virtual uint OnRead(Buffer& bs);

	// 发送数据
	virtual bool Send(const Buffer& bs);
	// 接收数据
	virtual uint Receive(Buffer& bs);
};

class EspTcp : public EspSocket
{
public:
	EspTcp(Esp8266& host);

	virtual String& ToStr(String& str) const { return str + "Tcp_" + Local.Port; }
};

class EspUdp : public EspSocket
{
public:
	EspUdp(Esp8266& host);

	virtual bool SendTo(const Buffer& bs, const IPEndPoint& remote);

	// 中断分发  维护状态
	virtual void OnProcess(byte reg);
	// 用户注册的中断事件处理 异步调用
	virtual void RaiseReceive();

	virtual String& ToStr(String& str) const { return str + "Udp_" + Local.Port; }

private:
	virtual bool OnWriteEx(const Buffer& bs, const void* opt);
};

String busy = "busy p...";
String discon = "WIFI DISCONNECT";
String conn = "WIFI CONNECTED";
String okstr = "OK";
String gotIp = "WIFI GOT IP";



/******************************** Esp8266 ********************************/

Esp8266::Esp8266(ITransport* port, Pin power, Pin rst)
{
	Set(port);

	if(power != P0) _power.Set(power);
	if(rst != P0) _rst.Set(rst);

	Led			= nullptr;
	NetReady	= nullptr;
	_Response	= nullptr;
}

void Esp8266::OpenAsync()
{
	if(Opened) return;

	Sys.AddTask([](void* param) { ((Esp8266*)param)->Open(); }, this, 0, -1, "Esp8266");
}

bool Esp8266::OnOpen()
{
	if(!PackPort::OnOpen()) return false;

	// 先关一会电，然后再上电，让它来一个完整的冷启动
	if(!_power.Empty()) _power.Down(10);

	// 每两次启动会有一次打开失败，交替
	if(!_rst.Empty())
	{
		_rst.Open();

		_rst = true;
		Sys.Sleep(10);
		_rst = false;
		//Sys.Sleep(100);
	}
	else
	{
		Reset();	// 软件重启命令
	}


	// 等待模块启动进入就绪状态
	if(!WaitForCmd("ready", 3000))
	{
		if (!Test())
		{
			net_printf("Esp8266::Open 打开失败！");

			return false;
		}
	}

	// 开回显
	SendCmd("ATE1");

	//UnJoinAP();
	AutoConn(false);

	// Station模式
	if (GetMode() != Modes::Station)
	{
		if(!SetMode(Modes::Station))
		{
			net_printf("设置Station模式失败！");

			return false;
		}
	}

	// 等待WiFi自动连接
	if(!WaitForCmd("WIFI CONNECTED", 3000))
	{
		//String ssid = "FAST_2.4G";
		//String pwd = "yws52718*";
		if (!JoinAP("yws007", "yws52718"))
		{
			net_printf("连接WiFi失败！\r\n");

			return false;
		}
	}

	// 发命令拿到IP地址

	return true;
}

void Esp8266::OnClose()
{
	_power.Close();
	_rst.Close();

	PackPort::OnClose();
}

// 配置网络参数
void Esp8266::Config()
{

}

ISocket* Esp8266::CreateSocket(ProtocolType type)
{
	switch(type)
	{
		case ProtocolType::Tcp:
			return new EspTcp(*this);

		case ProtocolType::Udp:
			return new EspUdp(*this);

		default:
			return nullptr;
	}
}

// 发送指令，在超时时间内等待返回期望字符串，然后返回内容
String Esp8266::Send(const String& cmd, const String& expect, uint msTimeout)
{
	TS("Esp8266::Send");

	String rs;

	// 在接收事件中拦截
	_Response	= &rs;

	if(cmd)
	{
		Port->Write(cmd);

#if NET_DEBUG
		net_printf("=> ");
		cmd.Trim().Show(true);
#endif
	}

	// 等待收到数据
	TimeWheel tw(0, msTimeout);
	tw.Sleep	= 100;
	while(rs.IndexOf(expect) < 0 && !tw.Expired());

	if(rs.Length() > 4) rs.Trim();

	_Response	= nullptr;

#if NET_DEBUG
	if(rs)
	{
		net_printf("<= ");
		rs.Trim().Show(true);
	}
#endif

	return rs;
}

// 引发数据到达事件
uint Esp8266::OnReceive(Buffer& bs, void* param)
{
	TS("Esp8266::OnReceive");

	// 拦截给同步方法
	if(_Response)
	{
		//_Response->Copy(_Response->Length(), bs.GetBuffer(), bs.Length());
		*_Response	+= bs.AsString();

		return 0;
	}
	bs.Show(true);

	return ITransport::OnReceive(bs, param);
}

// 发送命令，自动检测并加上\r\n，等待响应OK
bool Esp8266::SendCmd(const String& cmd, uint msTimeout)
{
	TS("Esp8266::SendCmd");

	static const String& expect("OK");

	String cmd2	= cmd;
	if(!cmd2.EndsWith("\r\n")) cmd2	+= "\r\n";

	auto rt	= Send(cmd2, expect, msTimeout);

	if(rt.IndexOf(expect) >= 0)  return true;

	// 设定小灯快闪时间，单位毫秒
	if(Led) Led->Write(50);

	return false;
}

bool Esp8266::WaitForCmd(const String& expect, uint msTimeout)
{
	TimeWheel tw(0, msTimeout);
	tw.Sleep	= 100;
	do
	{
		auto rs	= Send("", expect);
		if(rs && rs.IndexOf(expect) >= 0) return true;
	}
	while(!tw.Expired());

	return false;
}

/******************************** 基础AT指令 ********************************/

// 基础AT指令
bool Esp8266::Test()
{
	return SendCmd("AT");
}

bool Esp8266::Reset()
{
	return SendCmd("AT+RST");
}

String Esp8266::GetVersion()
{
	return Send("AT+GMR", "OK");
}

bool Esp8266::Sleep(uint ms)
{
	String cmd	= "AT+GSLP=";
	cmd	+= ms;
	return SendCmd(cmd);
}

/******************************** Esp8266 ********************************/

/*
发送：
"AT+CWMODE=1
"
返回：
"AT+CWMODE=1

OK
"
*/
bool Esp8266::SetMode(Modes mode)
{
	String cmd = "AT+CWMODE=";
	switch (mode)
	{
		case Modes::Station:
			cmd += '1';
			break;
		case Modes::Ap:
			cmd += '2';
			break;
		case Modes::Both:
			cmd += '3';
			break;
		case Modes::Unknown:
		default:
			return false;
	}
	if (!SendCmd(cmd)) return false;

	Mode = mode;

	return true;
}

/*
发送：
"AT+CWMODE?
"
返回：
"AT+CWMODE? +CWMODE:1

OK
"
*/
Esp8266::Modes Esp8266::GetMode()
{
	TS("Esp8266::GetMode");

	auto mode	= Send("AT+CWMODE?\r\n", "OK");
	if (!mode) return Modes::Unknown;

	Mode = Modes::Unknown;
	if (mode.IndexOf("+CWMODE:1") >= 0)
	{
		Mode = Modes::Station;
		net_printf("Modes::Station\r\n");
	}
	else if (mode.IndexOf("+CWMODE:2") >= 0)
	{
		Mode = Modes::Ap;
		net_printf("Modes::AP\r\n");
	}
	else if (mode.IndexOf("+CWMODE:3") >= 0)
	{
		Mode = Modes::Both;
		net_printf("Modes::Station+AP\r\n");
	}

	return Mode;
}

/*// 发送数据  并按照参数条件 适当返回收到的数据
void  Esp8266::Send(Buffer & dat)
{
	if (dat.Length() != 0)
	{
		Write(dat);
		// 仅仅 send 不需要刷出去
		//if (needRead)
		//	Port.Flush();
	}
}

int Esp8266::RevData(MemoryStream &ms, uint timeMs)
{
	byte temp[64];
	Buffer bf(temp, sizeof(temp));

	TimeWheel tw(0);
	tw.Reset(timeMs / 1000, timeMs % 1000);
	while (!tw.Expired())
	{
		// Port.Read 会修改 bf 的 length 为实际读到的数据长度
		// 所以每次都需要修改传入 buffer 的长度为有效值
		Sys.Sleep(10);
		bf.SetLength(sizeof(temp));
		Read(bf);
		if (bf.Length() != 0) ms.Write(bf);// 把读到的数据
	}

	return	ms.Position();
}

// str 不需要换行
bool Esp8266::SendCmd(String &str, uint timeOutMs)
{
	String cmd(str);
	cmd += "\r\n";
	Send(cmd);

	MemoryStream rsms;
	auto rslen = RevData(rsms, timeOutMs);
	debug_printf("\r\n ");
	str.Show(false);
	debug_printf(" SendCmd rev len:%d\r\n", rslen);
	if (rslen == 0)return false;

	String rsstr((const char *)rsms.GetBuffer(), rsms.Position());
	//rsstr.Show(true);

	bool isOK = true;
	int index = 0;
	int indexnow = 0;

	index = rsstr.IndexOf(str);
	if (index == -1)
	{
		// 没有找到命令本身就表示不通过
		isOK = false;
		debug_printf("not find cmd\r\n");
	}
	else
	{
		indexnow = index + str.Length();
		// 去掉busy
		index = rsstr.IndexOf(busy, indexnow);
		if (index != -1 && index - 5 < indexnow)
		{
			// 发现有 busy 并在允许范围就跳转    没有就不动作
			// debug_printf("find busy\r\n");
			indexnow = index + busy.Length();
		}

		index = rsstr.IndexOf(okstr, indexnow);
		if (index == -1 || index - 5 > indexnow)
		{
			// 没有发现  或者 不在范围内就认为是错的
			debug_printf("not find ok\r\n");
			isOK = false;
		}
		else
		{
			// 到达这里表示 得到正确的结果了
			indexnow = index + okstr.Length();
		}
	}
	// 还有多余数据 就扔出去
	//if(rslen -indexnow>5)xx()
	if (isOK)
		debug_printf(" Cmd OK\r\n");
	else
		debug_printf(" Cmd Fail\r\n");
	return isOK;
}*/

/*
发送：
"AT+CWJAP="yws007","yws52718"
"
返回：	( 一般3秒钟能搞定   密码后面位置会停顿，  WIFI GOT IP 前面也会停顿 )
"AT+CWJAP="yws007","yws52718"WIFI CONNECTED
WIFI GOT IP

OK
"
也可能  (已连接其他WIFI)  70bytes
"AT+CWJAP="yws007","yws52718" WIFI DISCONNECT
WIFI CONNECTED
WIFI GOT IP

OK
"
密码错误返回
"AT+CWJAP="yws007","7" +CWJAP:1

FAIL
"
*/
bool Esp8266::JoinAP(const String& ssid, const String& pwd)
{
	String cmd = "AT+CWJAP=";
	cmd = cmd + "\"" + ssid + "\",\"" + pwd + "\"\r\n";

	auto rs	= Send(cmd, "OK", 15000);

	int index = 0;
	int indexnow = 0;

	index = rs.IndexOf("AT+CWJAP");
	if (index != -1)
		indexnow = index+cmd.Length();
	else
	{
		debug_printf("not find cmd\r\n");
		return false;
	}

	/*// 补捞  重组字符串   因为不动rsms.Position()  所以数据保留不变
	if (cmd.Length()+discon.Length() > rslen)
	{
		rslen = RevData(rsms, 1000);
		debug_printf("\r\nRevData len:%d\r\n", rslen);
		rsstr = String((const char *)rsms.GetBuffer(), rsms.Position());
	}*/
	// 干掉 WIFI DISCONNECT
	index = rs.IndexOf(discon, indexnow);
	if (index != -1)indexnow = index + discon.Length();

	/*// 补捞  重组字符串   因为不动  rsms.Position() 所以数据保留不变
	auto comNum = index <= 0 ? cmd.Length() + conn.Length() : indexnow + conn.Length();
	if (comNum > rslen)
	{
		rslen = RevData(rsms, 1000);
		debug_printf("\r\nRevData len:%d\r\n", rslen);
		rsstr = String((const char *)rsms.GetBuffer(), rsms.Position());
	}*/

	index = rs.IndexOf(conn, indexnow);
	if (index == -1)
	{
		debug_printf("\r\nindex:%d,  find conn not ok\r\n",index);
		return false;
	}
	else
	{
		// 到达这里表示  已经连接到了
		indexnow = index + conn.Length();
	}

	/*// 补捞  重组字符串   因为不动  rsms.Position() 所以数据保留不变
	if (indexnow + 20 > rslen)
	{
		rslen = RevData(rsms, 1000);
		debug_printf("\r\nRevData len:%d\r\n", rslen);
		rsstr = String((const char *)rsms.GetBuffer(), rsms.Position());
	}*/

	index = rs.IndexOf(gotIp, indexnow);
	if (index == -1 || index - 5 > indexnow)
	{
		debug_printf("not find gotIp\r\n");
		return false;
	}
	else
	{
		// 到达这里表示  已经连接到了
		indexnow = index + gotIp.Length();
	}

	index = rs.IndexOf(okstr, indexnow);
	if (index == -1 || index - 5 > indexnow)
	{
		// 没有发现  或者 不在范围内就认为是错的
		debug_printf("not find ok\r\n");
		return false;
	}
	else
	{
		// 到达这里表示 得到正确的结果了
		indexnow = index + okstr.Length();
	}

	debug_printf("\r\nJoinAP  ok\r\n");
	return true;
}

/*
返回：
"AT+CWQAP WIFI DISCONNECT

OK
"
*/
bool Esp8266::UnJoinAP()
{
	String str = "AT+CWQAP";

	String cmd(str);
	cmd += "\r\n";
	auto rs	= Send(cmd, "AT+CWQAP");

	bool isOK = true;
	int index = 0;
	int indexnow = 0;

	index = rs.IndexOf(str);
	if (index == -1)
	{
		// 没有找到命令本身就表示不通过
		isOK = false;
		debug_printf("not find cmd\r\n");
	}
	else
	{
		indexnow = index + str.Length();
		// 去掉busy
		index = rs.IndexOf(busy, indexnow);
		if (index != -1 && index - 5 < indexnow)
		{
			// 发现有 busy 并在允许范围就跳转    没有就不动作
			// debug_printf("find busy\r\n");
			indexnow = index + busy.Length();
		}
		// 断开连接
		index = rs.IndexOf(discon, indexnow);
		if (index != -1 && index - 5 < indexnow)
		{
			// 断开连接
			indexnow = index + discon.Length();
		}
		// 去掉busy
		index = rs.IndexOf(busy, indexnow);
		if (index != -1 && index - 5 < indexnow)
		{
			indexnow = index + busy.Length();
		}

		index = rs.IndexOf(okstr, indexnow);
		if (index == -1 || index - 5 > indexnow)
		{
			// 没有发现  或者 不在范围内就认为是错的
			debug_printf("not find ok\r\n");
			isOK = false;
		}
		else
		{
			// 到达这里表示 得到正确的结果了
			indexnow = index + okstr.Length();
		}
	}
	// 还有多余数据 就扔出去
	//if(rslen -indexnow>5)xx()
	if (isOK)
		debug_printf(" Cmd OK\r\n");
	else
		debug_printf(" Cmd Fail\r\n");
	return isOK;
}

/*
开机自动连接WIFI
*/
bool Esp8266::AutoConn(bool enable)
{
	String cmd = "AT+CWAUTOCONN=";
	if (enable)
		cmd += "1";
	else
		cmd += "0";

	return SendCmd(cmd);
}


/******************************** Socket ********************************/

EspSocket::EspSocket(Esp8266& host, ProtocolType protocol)
	: _Host(host)
{
	Host		= &host;
	Protocol	= protocol;
}

EspSocket::~EspSocket()
{
	Close();
}

bool EspSocket::OnOpen()
{
	// 确保宿主打开
	if(!_Host.Open()) return false;

	// 如果没有指定本地端口，则使用累加端口
	if(!Local.Port)
	{
		// 累加端口
		static ushort g_port = 1024;
		if(g_port < 1024) g_port = 1024;
		Local.Port = g_port++;
	}
	Local.Address = _Host.IP;

#if DEBUG
	debug_printf("%s::Open ", Protocol == ProtocolType::Tcp ? "Tcp" : "Udp");
	Local.Show(false);
	debug_printf(" => ");
	Server.Show(false);
	debug_printf(" ");
	Remote.Show(true);
#endif

	String cmd	= "AT+CIPSTART=";

	if(Protocol == ProtocolType::Udp)
		cmd	+= "\"UDP\"";
	else if(Protocol == ProtocolType::Tcp)
		cmd	+= "\"TCP\"";

	auto rm	= Server;
	if(!rm) rm	= Remote.Address.ToString();

	// 设置端口目的(远程)IP地址和端口号
	cmd	= cmd + ",\"" + rm + "\"," + Remote.Port;
	// 设置自己的端口号
	cmd	= cmd + "," + Local.Port;
	// UDP传输属性。0，收到数据不改变远端目标；1，收到数据改变一次远端目标；2，收到数据改变远端目标
	cmd	= cmd + ",0";

	//如果Socket打开失败
	if(!_Host.SendCmd(cmd))
	{
		debug_printf("protocol %d, %d 打开失败 \r\n", Protocol, Remote.Port);
		OnClose();

		return false;
	}

	return true;
}

void EspSocket::OnClose()
{
	_Host.SendCmd("AT+CIPCLOSE");
}

/*// 应用配置，修改远程地址和端口
bool EspSocket::Change(const String& remote, ushort port)
{
	//if(!Close()) return false;

	RemoteDomain	= remote;
	Remote.Port		= port;

	// 可能这个字符串是IP地址，尝试解析
	auto ip	= IPAddress::Parse(remote);
	if(ip != IPAddress::Any()) Remote.Address	= ip;

	//if(!Open()) return false;
	return true;
}*/

// 接收数据
uint EspSocket::Receive(Buffer& bs)
{
	if(!Open()) return 0;

	/*// 读取收到数据容量
	ushort size = _REV16(SocRegRead2(RX_RSR));
	if(size == 0)
	{
		// 没有收到数据时，需要给缓冲区置零，否则系统逻辑会混乱
		bs.SetLength(0);
		return 0;
	}

	// 读取收到数据的首地址
	ushort offset = _REV16(SocRegRead2(RX_RD));

	// 长度受 bs 限制时 最大读取bs.Lenth
	if(size > bs.Length()) size = bs.Length();

	// 设置 实际要读的长度
	bs.SetLength(size);

	_Host.ReadFrame(offset, bs, Index, 0x03);

	// 更新实际物理地址,
	SocRegWrite2(RX_RD, _REV16(offset + size));
	// 生效 RX_RD
	WriteConfig(RECV);

	// 等待操作完成
	// while(ReadConfig());

	//返回接收到数据的长度
	return size;*/
	return 0;
}

// 发送数据
bool EspSocket::Send(const Buffer& bs)
{
	if(!Open()) return false;

	String cmd	= "AT+CIPSEND=0,";
	cmd	+= bs.Length();
	cmd	+= "\r\n";

	auto rt	= _Host.Send(cmd, ">");
	if(rt.IndexOf(">") < 0) return false;

	_Host.Send(bs.AsString(), "");

	return true;
}

bool EspSocket::OnWrite(const Buffer& bs) {	return Send(bs); }
uint EspSocket::OnRead(Buffer& bs) { return Receive(bs); }

/******************************** Tcp ********************************/

EspTcp::EspTcp(Esp8266& host)
	: EspSocket(host, ProtocolType::Tcp)
{

}

/******************************** Udp ********************************/

EspUdp::EspUdp(Esp8266& host)
	: EspSocket(host, ProtocolType::Udp)
{

}

bool EspUdp::SendTo(const Buffer& bs, const IPEndPoint& remote)
{
	if(remote == Remote) return Send(bs);

	if(!Open()) return false;

	String cmd	= "AT+CIPSEND=0,";
	cmd	+= bs.Length();

	// 加上远程IP和端口
	cmd	= cmd + "," + remote.Address;
	cmd	= cmd + "," + remote.Port;
	cmd	+= "\r\n";

	auto rt	= _Host.Send(cmd, ">");
	if(rt.IndexOf(">") < 0) return false;

	_Host.Send(bs.AsString(), "");

	return true;
}

bool EspUdp::OnWriteEx(const Buffer& bs, const void* opt)
{
	auto ep = (IPEndPoint*)opt;
	if(!ep) return OnWrite(bs);

	return SendTo(bs, *ep);
}

void EspUdp::OnProcess(byte reg)
{
	/*S_Interrupt ir;
	ir.Init(reg);
	// UDP 模式下只处理 SendOK  Recv 两种情况

	if(ir.RECV)
	{
		RaiseReceive();
	}*/
	//	SEND OK   不需要处理 但要清空中断位
}

// UDP 异步只有一种情况  收到数据  可能有多个数据包
// UDP接收到的数据结构： RemoteIP(4 byte) + RemotePort(2 byte) + Length(2 byte) + Data(Length byte)
void EspUdp::RaiseReceive()
{
	/*byte buf[1500];
	Buffer bs(buf, ArrayLength(buf));
	ushort size = Receive(bs);
	Stream ms(bs.GetBuffer(), size);

	// 拆包
	while(ms.Remain())
	{
		IPEndPoint ep	= ms.ReadArray(6);
		ep.Port = _REV16(ep.Port);

		ushort len = ms.ReadUInt16();
		len = _REV16(len);
		// 数据长度不对可能是数据错位引起的，直接丢弃数据包
		if(len > 1500)
		{
			net_printf("W5500 UDP数据接收有误, ep=%s Length=%d \r\n", ep.ToString().GetBuffer(), len);
			return;
		}
		// 回调中断
		Buffer bs3(ms.ReadBytes(len), len);
		OnReceive(bs3, &ep);
	};*/
}
