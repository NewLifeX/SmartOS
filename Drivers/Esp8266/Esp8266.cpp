#include "Sys.h"
#include "Task.h"
#include "Time.h"

#include "Esp8266.h"
#include "EspTcp.h"
#include "EspUdp.h"
#include "WaitExpect.h"

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

/******************************** Esp8266 ********************************/

Esp8266::Esp8266(ITransport* port, Pin power, Pin rst)
{
	Set(port);

	if(power != P0) _power.Init(power, false);
	if(rst != P0) _rst.Init(rst, true);

	AutoConn	= false;

	Led			= nullptr;
	NetReady	= nullptr;

	_Expect		= nullptr;

	Buffer(_sockets, 5 * 4).Clear();

	Mode	= SocketMode::STA_AP;

	SSID	= new String();
	Pass	= new String();
}

Esp8266::~Esp8266()
{
	delete SSID;
	delete Pass;
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
	if (!_power.Empty())
	{
		_power.Open();					// 使用前必须Open；
		_power.Down(20);
	}
	if (!_rst.Empty()) _rst.Open();		// 使用前必须Open；

	// 每两次启动会有一次打开失败，交替
	if(!_rst.Empty())
		Reset(false);	// 硬重启
	else
		Reset(true);	// 软件重启命令

	// 如果首次加载，则说明现在处于出厂设置模式，需要对模块恢复出厂设置
	auto cfg	= Config::Current->Find("NET");
	//if(!cfg) Restore();

	for(int i=0; i<2; i++)
	{
		// 等待模块启动进入就绪状态
		if(!WaitForCmd("ready", 3000))
		{
			if (!Test())
			{
				net_printf("Esp8266::Open 打开失败！");

				return false;
			}
		}
		if(cfg || i==1) break;

		Restore();
	}

	// 开回显
	Echo(true);

#if NET_DEBUG
	// 获取版本
	auto ver	= GetVersion();
	//net_printf("版本:");
	//ver.Show(true);
#endif

	// 默认Both
	if(Mode == SocketMode::Wire) Mode	= SocketMode::STA_AP;

	if (GetMode() != Mode)
	{
		if(!SetMode(Mode))
		{
			net_printf("设置Station模式失败！");

			return false;
		}
	}

	Config();

	SetDHCP(Mode, true);

	// 等待WiFi自动连接
	if(!AutoConn || !WaitForCmd("WIFI CONNECTED", 3000))
	{
		if (!SSID || !*SSID || Mode == SocketMode::STA_AP)
		{
			net_printf("启动AP模式!\r\n");
			String wifiName = "WsLink-";
			wifiName += Buffer(Sys.ID, 3).ToHex();

			int chn	= (Sys.Ms() % 14) + 1;
			SetAP(wifiName, "", chn, 0, 1, 1);
		}
		if (SSID && *SSID)
		{
			if (!JoinAP(*SSID, *Pass))	// Pass 可以为空
			{
				net_printf("连接WiFi失败！\r\n");
				return false;
			}
		}
	}

	// 拿到IP，网络就绪
	if(Mode == SocketMode::Station || Mode == SocketMode::STA_AP)
	{
		IP	= GetIP(true);

		SaveConfig();
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
	// 设置多连接
	SetMux(true);

	// 设置IPD
	SetIPD(true);

	auto mac	= Mac;
	SetMAC(true, mac);
	mac[5]++;
	SetMAC(false, mac);

	SetAutoConn(AutoConn);
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

// 启用DNS
bool Esp8266::EnableDNS() { return true; }
// 启用DHCP
bool Esp8266::EnableDHCP() { Mode	= SocketMode::STA_AP;	return true;/*  return SetDHCP(SocketMode::Both, true); */}

// 发送指令，在超时时间内等待返回期望字符串，然后返回内容
String Esp8266::Send(const String& cmd, cstring expect, cstring expect2, uint msTimeout)
{
	TS("Esp8266::Send");

	String rs;

	auto tid	= Task::Scheduler()->Current->ID;
	// 判断是否正在发送其它指令
	if(_Expect)
	{
#if NET_DEBUG
		auto w	= (WaitExpect*)_Expect;
		net_printf("Esp8266::Send %d 正在发送 ", w->TaskID);
		if(w->Command)
			w->Command->Trim().Show(false);
		else
			net_printf("数据");
		net_printf(" ，%d 无法发送 ", tid);
		cmd.Trim().Show(true);
#endif

		return rs;
	}

	// 在接收事件中拦截
	WaitExpect we;
	we.TaskID	= tid;
	//we.Command	= &cmd;
	we.Result	= &rs;
	we.Key1		= expect;
	we.Key2		= expect2;

	_Expect	= &we;

#if NET_DEBUG
	bool EnableLog	= true;
#endif

	if(cmd)
	{
		// 设定小灯快闪时间，单位毫秒
		if(Led) Led->Write(50);

		Port->Write(cmd);

#if NET_DEBUG
		// 只有AT指令显示日志
		if(!cmd.StartsWith("AT") || expect && expect[0] == '>') EnableLog = false;
		if(EnableLog)
		{
			we.Command	= &cmd;
			net_printf("%d=> ", tid);
			cmd.Trim().Show(true);
		}
#endif
	}

	we.Wait(msTimeout);
	if(_Expect	== &we) _Expect	= nullptr;

	//if(rs.Length() > 4) rs.Trim();

#if NET_DEBUG
	if(EnableLog && rs)
	{
		net_printf("%d<= ", tid);
		rs.Trim().Show(true);
	}
#endif

	return rs;
}

// 发送命令，自动检测并加上\r\n，等待响应OK
bool Esp8266::SendCmd(const String& cmd, uint msTimeout)
{
	TS("Esp8266::SendCmd");

	static const cstring ok		= "OK";
	static const cstring err	= "ERROR";

	String cmd2;

	// 只有AT指令需要检查结尾，其它指令不检查，避免产生拷贝
	auto p	= &cmd;
	if(cmd.StartsWith("AT") && !cmd.EndsWith("\r\n"))
	{
		cmd2	= cmd;
		cmd2	+= "\r\n";
		p	= &cmd2;
	}

	// 二级拦截。遇到错误也马上结束
	auto rt	= Send(*p, ok, err, msTimeout);

	return rt.Contains(ok);
}

bool Esp8266::WaitForCmd(cstring expect, uint msTimeout)
{
	String rs;

	// 在接收事件中拦截
	WaitExpect we;
	we.Result	= &rs;
	we.Key1		= expect;
	we.Capture	= false;

	_Expect	= &we;

	// 等待收到数据
	bool rt	= we.Wait(msTimeout);
	_Expect	= nullptr;

	return rt;
}

void ParseFail(cstring name, const Buffer& bs)
{
#if NET_DEBUG
	if(bs.Length() == 0) return;

	int p	= 0;
	if(p < bs.Length() && bs[p] == ' ') p++;
	if(p < bs.Length() && bs[p] == '\r') p++;
	if(p < bs.Length() && bs[p] == '\n') p++;

	// 无法识别的数据可能是空格前缀，需要特殊处理
	auto str	= bs.Sub(p, -1).AsString();
	if(str)
	{
		net_printf("Esp8266:%s 无法识别[%d]：", name, bs.Length());
		//if(bs.Length() == 2) net_printf("%02X %02X ", bs[0], bs[1]);
		str.Show(true);
	}
#endif
}

// 引发数据到达事件
uint Esp8266::OnReceive(Buffer& bs, void* param)
{
	if(bs.Length() == 0) return 0;

	TS("Esp8266::OnReceive");

	//!!! 分析+IPD数据和命令返回，特别要注意粘包
	int s	= 0;
	int p	= 0;
	auto str	= bs.AsString();
	while(p >= 0 && p < bs.Length())
	{
		s	= p;
		p	= str.IndexOf("+IPD,", s);

		// +IPD之前之后的数据，留给命令分析
		int size	= p>=0 ? p-s : bs.Length()-s;
		if(size > 0)
		{
			if(_Expect)
			{
				uint rs	= ParseReply(bs.Sub(s, size));
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
		if(p >= 0)
		{
			if(p + 5 >= bs.Length())
			{
#if NET_DEBUG
				ParseFail("+IPD<=5", bs.Sub(p, -1));
#endif
				break;
			}
			else
			{
				uint rs	= ParseReceive(bs.Sub(p, -1));
				if(rs <= 0)
				{
#if NET_DEBUG
					ParseFail("ParseReceive", bs.Sub(p + rs, -1));
#endif
					break;
				}

				// 游标移到下一组数据
				p	+= rs;
			}
		}
	}

	return 0;
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

// 分析+IPD接收数据。返回被用掉的字节数
uint Esp8266::ParseReceive(const Buffer& bs)
{
	TS("Esp8266::ParseReceive");

	auto str	= bs.AsString();

	StringSplit sp(str, ",");

	// 跳过第一段+IPD
	auto rs	= sp.Next();

	if(!(rs	= sp.Next())) return 0;
	int idx	= rs.ToInt();
	if(idx < 0 || idx > 4) return 0;

	if(!(rs	= sp.Next())) return 0;
	int len	= rs.ToInt();

	IPEndPoint ep;
	if(!(rs	= sp.Next())) return 0;
	ep.Address	= IPAddress::Parse(rs);

	// 提前改变下一个分隔符
	sp.Sep	= ":";
	if(!(rs	= sp.Next())) return 0;
	ep.Port	= rs.ToInt();

	// 后面是数据
	sp.Next();
	int s	= sp.Position;
	if(s <= 0) return 0;

	// 分发给各个Socket
	auto es	= (EspSocket**)_sockets;
	auto sk	= es[idx];
	if(sk)
	{
		// 校验数据长度
		int len2	= len;
		int remain	= bs.Length() - s;
		if(remain < len2)
		{
			len2	= remain;
			net_printf("剩余数据长度 %d 小于标称长度 %d \r\n", remain, len);
		}

		sk->OnProcess(bs.Sub(s, len2), ep);
	}

	return sp.Position + len;
}

// 分析关键字。返回被用掉的字节数
uint Esp8266::ParseReply(const Buffer& bs)
{
	if(!_Expect) return 0;

	// 拦截给同步方法
	auto we	= (WaitExpect*)_Expect;
	bool rs	= we->Parse(bs);

	// 如果内部已经适配，则清空
	if(!we->Result) _Expect	= nullptr;

	return rs;
}

/******************************** 基础AT指令 ********************************/

// 基础AT指令
bool Esp8266::Test()
{
	for(int i=0; i<10; i++)
	{
		if(SendCmd("AT", 500)) return true;

		Reset(false);
	}

	return false;
}

bool Esp8266::Reset(bool soft)
{
	if(soft)
		return SendCmd("AT+RST");
	else
	{
		_rst.Up(100);
		return true;
	}
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
bool Esp8266::SetMode(SocketMode mode)
{
	String cmd = "AT+CWMODE=";
	cmd	+= (byte)mode;

	if (!SendCmd(cmd)) return false;

	Mode	= mode;

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
SocketMode Esp8266::GetMode()
{
	TS("Esp8266::GetMode");

	auto mod	= SocketMode::Station;

	auto rs	= Send("AT+CWMODE?\r\n", "OK");
	if (!rs) return mod;

	int p	= rs.IndexOf(':');
	if(p < 0) return mod;

	return	(SocketMode)rs.Substring(p+1, 1).ToInt();
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
#if NET_DEBUG
	TimeCost tc;
#endif

	String cmd = "AT+CWJAP=";
	cmd = cmd + "\"" + ssid + "\",\"" + pass + "\"";

	bool rs	= SendCmd(cmd, 15000);

#if NET_DEBUG
	net_printf("Esp8266::JoinAP ");
	if(rs)
		net_printf("成功 ");
	else
		net_printf("失败 ");
	tc.Show();
#endif

	return rs;
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
	cmd = cmd + "\"" + ssid + "\",\"" + pass + "\"," + channel + ',' + ecn /*+ ',' + maxConnect + ',' + (hidden ? '1' : '0')*/;

	return SendCmd(cmd, 1600);
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

bool Esp8266::SetDHCP(SocketMode mode, bool enable)
{
	byte m	= 0;
	switch(mode)
	{
		case SocketMode::Station:
			m	= 1;
			break;
		case SocketMode::AP:
			m	= 0;
			break;
		case SocketMode::STA_AP:
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

// 设置MAC会导致WiFi连接断开
bool Esp8266::SetMAC(bool sta, const MacAddress& mac)
{
	String cmd	= sta ? "AT+CIPSTAMAC" : "AT+CIPAPMAC";
	cmd = cmd + "=\"" + mac.ToString().Replace('-', ':') + '\"';

	return SendCmd(cmd);
}

IPAddress Esp8266::GetIP(bool sta)
{
	auto rs	= Send(sta ? "AT+CIPSTA?\r\n" : "AT+CIPAP?\r\n", "OK");
	int p	= rs.IndexOf("ip:\"");
	if(p < 0) return IPAddress::Any();

	p	+= 4;
	int e	= rs.IndexOf("\"", p);
	if(e < 0) return IPAddress::Any();

	return IPAddress::Parse(rs.Substring(p, e - p));
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

bool Esp8266::SetIPD(bool enable)
{
	String cmd = "AT+CIPDINFO=";
	return SendCmd(cmd + (enable ? '1' : '0'));
}
