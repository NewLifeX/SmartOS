#include "Time.h"
#include "Message\DataStore.h"

#include "Esp8266.h"
#include "Config.h"

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
	byte		_Index;

public:
	EspSocket(Esp8266& host, ProtocolType protocol, byte idx);
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

	// 收到数据
	virtual void OnProcess(const Buffer& bs, const IPEndPoint& remote);
};

class EspTcp : public EspSocket
{
public:
	EspTcp(Esp8266& host, byte idx);

	virtual String& ToStr(String& str) const { return str + "Tcp_" + Local.Port; }
};

class EspUdp : public EspSocket
{
public:
	EspUdp(Esp8266& host, byte idx);

	virtual bool SendTo(const Buffer& bs, const IPEndPoint& remote);

	virtual String& ToStr(String& str) const { return str + "Udp_" + Local.Port; }

private:
	virtual bool OnWriteEx(const Buffer& bs, const void* opt);
};

class EspConfig : public ConfigBase
{
public:
	byte	Length;			// 数据长度

	char	_SSID[32];		// 登录名
	char	_Pass[32];		// 登录密码

	byte	TagEnd;		// 数据区结束标识符

	String	SSID;
	String	Pass;

	EspConfig();
	virtual void Init();
	virtual void Load();
	virtual void Show() const;

	static EspConfig* Create();
};

/******************************** Esp8266 ********************************/

Esp8266::Esp8266(ITransport* port, Pin power, Pin rst)
{
	Set(port);

	if(power != P0) _power.Set(power);
	if(rst != P0) _rst.Set(rst);

	//Mode		= Modes::Both;
	AutoConn	= false;

	Led			= nullptr;
	NetReady	= nullptr;
	_Response	= nullptr;

	Buffer(_sockets, 5 * 4).Clear();

	Wireless	= (byte)Modes::Both;
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
	Echo(true);

#if NET_DEBUG
	// 获取版本
	auto ver	= GetVersion();
	net_printf("版本:");
	ver.Show(true);
#endif

	auto cfg	= EspConfig::Create();

	//UnJoinAP();

	// Station模式
	auto mode	= (Modes)Wireless;
	// 默认Both
	if(mode == Modes::Unknown) mode	= Modes::Both;

	if (GetMode() != mode)
	{
		if(!SetMode(mode))
		{
			net_printf("设置Station模式失败！");

			return false;
		}
	}

	SetAutoConn(AutoConn);
	// 等待WiFi自动连接
	if(!AutoConn || !WaitForCmd("WIFI CONNECTED", 3000))
	{
		//String ssid = "FAST_2.4G";
		//String pwd = "yws52718*";
		//if (!JoinAP("yws007", "yws52718"))
		//if (!cfg->SSID && !cfg->Pass && !JoinAP(cfg->SSID, cfg->Pass))
		if (SSID && Pass && !JoinAP(SSID, Pass))
		{
			net_printf("连接WiFi失败！\r\n");

			return false;
		}
	}

	// 设置多连接
	SetMux(true);

	// 设置IPD
	SetIPD(true);

	// 拿到IP，网络就绪
	if(mode == Modes::Station || mode == Modes::Both)
	{
		IP	= GetIP(true);

		ShowConfig();
	}

	if(NetReady) NetReady(this);

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
	auto es	= (EspSocket**)_sockets;

	int i = 0;
	for(i = 0; i < 5; i++)
	{
		if(es[i] == nullptr) break;
	}
	if(i >= 5 )
	{
		net_printf("没有空余的Socket可用了 !\r\n");

		return nullptr;
	}

	switch(type)
	{
		case ProtocolType::Tcp:
			return es[i]	= new EspTcp(*this, i);

		case ProtocolType::Udp:
			return es[i]	= new EspUdp(*this, i);

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

/*
+IPD – 接收网络数据
1) 单连接时:
(+CIPMUX=0)
+IPD,<len>[,<remote IP>,<remote
port>]:<data>
2) 多连接时：
(+CIPMUX=1)
+IPD,<link ID>,<len>[,<remote IP>,<remote port>]:<data>
*/
// 引发数据到达事件
uint Esp8266::OnReceive(Buffer& bs, void* param)
{
	TS("Esp8266::OnReceive");

	auto str	= bs.AsString();

	// +IPD开头的是收到网络数据
	if(str.StartsWith("+IPD,"))
	{
		int s	= str.IndexOf(",") + 1;
		int e	= str.IndexOf(",", s);

		int idx	= str.Substring(s, e - s).ToInt();

		s	= e + 1;
		e	= str.IndexOf(",", s);
		int len	= str.Substring(s, e - s).ToInt();

		IPEndPoint ep;

		s	= e + 1;
		e	= str.IndexOf(",", s);
		ep.Address	= IPAddress::Parse(str.Substring(s, e - s));

		s	= e + 1;
		e	= str.IndexOf(":", s);
		ep.Port		= str.Substring(s, e - s).ToInt();

		// 后面是数据
		s	= e + 1;

		// 分发给各个Socket
		auto es	= (EspSocket**)_sockets;
		auto sk	= es[idx];
		if(!sk) sk->OnProcess(bs.Sub(s, -1), ep);
	}

	// 拦截给同步方法
	if(_Response)
	{
		//_Response->Copy(_Response->Length(), bs.GetBuffer(), bs.Length());
		*_Response	+= str;

		return 0;
	}
	bs.Show(true);

	return ITransport::OnReceive(bs, param);
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

/*
AT 版本信息
基于的SDK版本信息
编译生成时间
*/
String Esp8266::GetVersion()
{
	return Send("AT+GMR\r\n", "OK");
}

bool Esp8266::Sleep(uint ms)
{
	String cmd	= "AT+GSLP=";
	cmd	+= ms;
	return SendCmd(cmd);
}

bool Esp8266::Echo(bool open)
{
	String cmd	= "ATE";
	cmd	= cmd + (open ? '1' : '0');
	return SendCmd(cmd);
}

// 恢复出厂设置，将擦写所有保存到Flash的参数，恢复为默认参数。会导致模块重启
bool Esp8266::Restore()
{
	return SendCmd("AT+RESTORE");
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
	cmd	+= (byte)mode;

	if (!SendCmd(cmd)) return false;

	Wireless = (byte)mode;

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

	auto mod	= Modes::Unknown;

	auto rs	= Send("AT+CWMODE?\r\n", "OK");
	if (!rs) return mod;

	/*Modes mod;
	if (mode.IndexOf("+CWMODE:1") >= 0)
	{
		mod = Modes::Station;
		net_printf("Modes::Station\r\n");
	}
	else if (mode.IndexOf("+CWMODE:2") >= 0)
	{
		mod = Modes::Ap;
		net_printf("Modes::AP\r\n");
	}
	else if (mode.IndexOf("+CWMODE:3") >= 0)
	{
		mod = Modes::Both;
		net_printf("Modes::Station+AP\r\n");
	}*/

	int p	= rs.IndexOf(':');
	if(p < 0) return mod;

	mod	=(Modes)rs.Substring(p+1, 1).ToInt();

	Wireless	= (byte)mod;

	return mod;
}

// +CWJAP:<ssid>,<bssid>,<channel>,<rssi>
// <bssid>目标AP的MAC地址
String Esp8266::GetJoinAP()
{
	return Send("AT+CWJAP?\r\n", "OK");
}

/*
发送：
"AT+CWJAP="yws007','yws52718"
"
返回：	( 一般3秒钟能搞定   密码后面位置会停顿，  WIFI GOT IP 前面也会停顿 )
"AT+CWJAP="yws007','yws52718"WIFI CONNECTED
WIFI GOT IP

OK
"
也可能  (已连接其他WIFI)  70bytes
"AT+CWJAP="yws007','yws52718" WIFI DISCONNECT
WIFI CONNECTED
WIFI GOT IP

OK
"
密码错误返回
"AT+CWJAP="yws007','7" +CWJAP:1

FAIL
"
*/
bool Esp8266::JoinAP(const String& ssid, const String& pass)
{
	String cmd = "AT+CWJAP=";
	cmd = cmd + "\"" + ssid + "\",\"" + pass + "\"";

	return SendCmd(cmd, 15000);
}

/*
返回：
"AT+CWQAP WIFI DISCONNECT

OK
"
*/
bool Esp8266::UnJoinAP()
{
	return SendCmd("AT+CWQAP", 2000);
}

/*
开机自动连接WIFI
*/
bool Esp8266::SetAutoConn(bool enable)
{
	String cmd = "AT+CWAUTOCONN=";

	return SendCmd(cmd + (enable ? '1' : '0'));
}

// +CWLAP:<enc>,<ssid>,<rssi>,<mac>,<ch>,<freq offset>,<freq calibration>
// freq offset, AP频偏，单位kHz，转为成ppm需要除以2.4
// freq calibration，频偏校准值
String Esp8266::LoadAPs()
{
	return Send("AT+CWLAP\r\n", "OK");
}

// +CWSAP:<ssid>,<pwd>,<chl>,<ecn>,<max conn>,<ssid hidden>
String Esp8266::GetAP()
{
	return Send("AT+CWSAP\r\n", "OK");
}

/*
注意: 指令只有在 softAP 模式开启后有效
参数说明：
<ssid> 字符串参数，接入点名称
<pwd> 字符串参数，密码强度范围：8 ~ 64 字节 ASCII
<chl> 通道号
<ecn> 加密方式，不支持 WEP
   0 OPEN
   2 WPA_PSK
   3 WPA2_PSK
   4 WPA_WPA2_PSK
<max conn> 允许连接 ESP8266 soft-AP 的最多 station 数目
   取值范围 [1, 4]
<ssid hidden> 默认为 0，开启广播 ESP8266 soft-AP SSID
  0 广播 SSID
  1 不广播 SSID
*/
bool Esp8266::SetAP(const String& ssid, const String& pass, byte channel, byte ecn, byte maxConnect, bool hidden)
{
	String cmd = "AT+CWSAP=";
	cmd = cmd + "\"" + ssid + "\",\"" + pass + "\"," + channel + ',' + ecn + ',' + maxConnect + ',' + (hidden ? '1' : '0');

	return SendCmd(cmd, 15000);
}

// <ip addr>,<mac>
// 查询连接到AP的Stations信息。无法查询DHCP接入
String Esp8266::LoadStations()
{
	return Send("AT+CWLIF\r\n", "OK");
}

bool Esp8266::GetDHCP(bool* sta, bool* ap)
{
	auto rs	= Send("AT+CWDHCP?\r\n", "OK");
	if(!rs) return false;

	byte n	= rs.ToInt();
	*sta	= n & 0x01;
	*ap		= n & 0x02;

	return true;
}

bool Esp8266::SetDHCP(Modes mode, bool enable)
{
	byte m	= 0;
	switch(mode)
	{
		case Modes::Station:
			m	= 1;
			break;
		case Modes::Ap:
			m	= 0;
			break;
		case Modes::Both:
			m	= 2;
			break;
		default:
			return false;
	}

	String cmd = "AT+CWDHCP=";
	return SendCmd(cmd + m + ',' + enable);
}

MacAddress Esp8266::GetMAC(bool sta)
{
	auto rs	= Send(sta ? "AT+CIPSTAMAC?\r\n" : "AT+CIPAPMAC?\r\n", "OK");
	int p	= rs.IndexOf(':');
	if(p < 0) return MacAddress::Empty();

	return MacAddress::Parse(rs.Substring(p + 1, -1));
}

bool Esp8266::SetMAC(bool sta, const MacAddress& mac)
{
	String cmd	= sta ? "AT+CIPSTAMAC=" : "AT+CIPAPMAC=";
	cmd = cmd + "=\"" + mac + '\"';

	return SendCmd(cmd);
}

IPAddress Esp8266::GetIP(bool sta)
{
	auto rs	= Send(sta ? "AT+CIPSTA?\r\n" : "AT+CIPAP?\r\n", "OK");
	int p	= rs.IndexOf(':');
	if(p < 0) return IPAddress::Any();

	return IPAddress::Parse(rs.Substring(p + 1, -1));
}

/******************************** TCP/IP ********************************/

/*
STATUS:<stat>
+CIPSTATUS:<link ID>,<type>,<remote IP>,<remote port> ,<local port>,<tetype>

参数说明：
<stat>
     2：获得 IP
     3：已连接
     4：断开连接
     5：未连接到 WiFi
<link ID> ⺴˷	络连接 ID (0~4)，⽤ݒ于多连接的情况
<type> 字符串参数， “TCP” 或者 “UDP”
<remote IP> 字符串，远端 IP 地址
<remote port> 远端端⼝Ծ值
<local port> ESP8266 本地端⼝Ծ值
<tetype>
     0: ESP8266 作为 client
     1: ESP8266 作为 server
*/
String Esp8266::GetStatus()
{
	return Send("AT+CIPSTATUS?\r\n", "OK");
}

bool Esp8266::GetMux()
{
	auto rs	= Send("AT+CIPMUX?\r\n", "OK");
	int p	= rs.IndexOf(':');
	if(p < 0) return false;

	return rs.Substring(p + 1, 1) != "0";
}

bool Esp8266::SetMux(bool enable)
{
	// 多连接要求非透传模式
	if(!enable || !SendCmd("AT+CIPMODE=0")) return false;

	String cmd = "AT+CIPMUX=";
	return SendCmd(cmd + (enable ? '1' : '0'));
}

bool Esp8266::Update()
{
	return SendCmd("AT+CIPUPDATE");
}

bool Esp8266::Ping(const IPAddress& ip)
{
	String cmd	= "AT+PING=";

	return SendCmd(cmd + ip);
}

/*
+IPD – 接收网络数据
1) 单连接时:
(+CIPMUX=0)
+IPD,<len>[,<remote IP>,<remote
port>]:<data>
2) 多连接时：
(+CIPMUX=1)
+IPD,<link ID>,<len>[,<remote IP>,<remote port>]:<data>
说明:
此指令在普通指令模式下有效，ESP8266 接收到⺴˷	络数据时向串⼝Ծ发
送 +IPD 和数据
[<remote IP>] ⺴˷	络通信对端 IP，由指令“AT+CIPDINFO=1”使能显⽰ޓ
[<remote port>] ⺴˷	络通信对端端⼝Ծ，由指令“AT+CIPDINFO=1”使能
<link ID> 收到⺴˷	络连接的 ID 号
<len> 数据⻓ॗ度
<data> 收到的数据
*/
bool Esp8266::SetIPD(bool enable)
{
	String cmd = "AT+CIPDINFO=";
	return SendCmd(cmd + (enable ? '1' : '0'));
}

/******************************** Socket ********************************/

EspSocket::EspSocket(Esp8266& host, ProtocolType protocol, byte idx)
	: _Host(host)
{
	_Index		= idx;

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

	cmd	= cmd + _Index + ",";

	if(Protocol == ProtocolType::Udp)
		cmd	+= "\"UDP\"";
	else if(Protocol == ProtocolType::Tcp)
		cmd	+= "\"TCP\"";

	auto rm	= Server;
	if(!rm) rm	= Remote.Address.ToString();

	// 设置端口目的(远程)IP地址和端口号
	cmd	= cmd + ",\"" + rm + "\"," + Remote.Port;
	// 设置自己的端口号
	if(Local.Port) cmd	= cmd + ',' + Local.Port;
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
	String cmd	= "AT+CIPCLOSE=";
	cmd += _Index;

	_Host.SendCmd(cmd);
}

// 接收数据
uint EspSocket::Receive(Buffer& bs)
{
	if(!Open()) return 0;

	return 0;
}

// 发送数据
bool EspSocket::Send(const Buffer& bs)
{
	if(!Open()) return false;

	String cmd	= "AT+CIPSEND=";
	cmd = cmd + _Index + ',' + bs.Length() + "\r\n";

	auto rt	= _Host.Send(cmd, ">");
	if(rt.IndexOf(">") < 0) return false;

	_Host.Send(bs.AsString(), "");

	return true;
}

bool EspSocket::OnWrite(const Buffer& bs) {	return Send(bs); }
uint EspSocket::OnRead(Buffer& bs) { return Receive(bs); }

// 收到数据
void EspSocket::OnProcess(const Buffer& bs, const IPEndPoint& remote)
{
	OnReceive((Buffer&)bs, (void*)&remote);
}

/******************************** Tcp ********************************/

EspTcp::EspTcp(Esp8266& host, byte idx)
	: EspSocket(host, ProtocolType::Tcp, idx)
{

}

/******************************** Udp ********************************/

EspUdp::EspUdp(Esp8266& host, byte idx)
	: EspSocket(host, ProtocolType::Udp, idx)
{

}

bool EspUdp::SendTo(const Buffer& bs, const IPEndPoint& remote)
{
	if(remote == Remote) return Send(bs);

	if(!Open()) return false;

	String cmd	= "AT+CIPSEND=";
	cmd = cmd + _Index + ',' + bs.Length();

	// 加上远程IP和端口
	cmd	= cmd + ',' + remote.Address;
	cmd	= cmd + ',' + remote.Port;
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

/******************************** EspConfig ********************************/

EspConfig::EspConfig() : ConfigBase(),
	SSID(_SSID, sizeof(_SSID)),
	Pass(_Pass, sizeof(_Pass))
{
	_Name	 = "EspCfg";
	_Start	 = &Length;
	_End	 = &TagEnd;
	Init();
}

void EspConfig::Init()
{
	ConfigBase::Init();

	Length		= Size();
}

void EspConfig::Load()
{
	ConfigBase::Load();

	SSID	= _SSID;
	Pass	= _Pass;
}

void EspConfig::Show() const
{
#if DEBUG
	debug_printf("EspConfig::无线配置：\r\n");

	debug_printf("\tSSID: %s \r\n", SSID.GetBuffer());
	debug_printf("\t密码: %s \r\n", Pass.GetBuffer());
#endif
}

EspConfig* EspConfig::Create()
{
	static EspConfig cfg;
	if(cfg.New)
	{
		cfg.Init();
		cfg.Load();

		cfg.New	= false;
	}

	return &cfg;
}
