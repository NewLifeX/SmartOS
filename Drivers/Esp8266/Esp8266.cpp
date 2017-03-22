#include "Kernel\Sys.h"
#include "Kernel\Task.h"
#include "Kernel\TTime.h"

#include "Device\SerialPort.h"

#include "Esp8266.h"
#include "EspTcp.h"
#include "EspUdp.h"

#include "Config.h"

#include "App\FlushPort.h"

#define NET_DEBUG DEBUG
//#define NET_DEBUG 0
#if NET_DEBUG
#define net_printf debug_printf
#else
#define net_printf(format, ...)
#endif

void LoadStationTask(void* param);

/*
		注意事项
1、设置模式AT+CWMODE需要重启后生效AT+RST
2、AP模式下查询本机IP无效，可能会造成死机
3、开启server需要多连接作为基础AT+CIPMUX=1
4、单连接模式，多连接模式  收发数据有参数个数区别
*/

/******************************** Esp8266 ********************************/

Esp8266::Esp8266()
{
	Name = "Esp8266";
	Speed = 54;

	_task = 0;

	Led = nullptr;

	Buffer(_sockets, 5 * 4).Clear();

	Mode = NetworkType::STA_AP;
	WorkMode = NetworkType::STA_AP;

	At.DataKey = "+IPD,";

	InitConfig();
	LoadConfig();

	// 配置模式作为工作模式
	WorkMode = Mode;
}

Esp8266::~Esp8266()
{
	RemoveLed();
}

void Esp8266::Init(COM idx, int baudrate)
{
	At.Init(idx, baudrate);
}

void Esp8266::Init(ITransport* port)
{
	At.Init(port);
}

void Esp8266::Set(Pin power, Pin rst, Pin low)
{
	if (power != P0) _Power.Set(power);
	if (rst != P0) _Reset.Set(rst);
	if (low != P0) _LowPower.Set(low);
}

void Esp8266::SetLed(Pin led)
{
	if (led != P0)
	{
		auto port = new OutputPort(led);
		SetLed(*port);
	}
}

void Esp8266::SetLed(OutputPort& led)
{
	auto fp = new FlushPort();
	fp->Port = &led;
	fp->Start();
	Led = fp;
}

void Esp8266::RemoveLed()
{
	if (Led)delete (FlushPort*)Led;
	Led = nullptr;
}

bool Esp8266::OnOpen()
{
	if (!At.Open()) return false;

	if (!Test(1, 1000) && !CheckReady())
	//if (!CheckReady())
	{
		net_printf("Esp8266::Open 打开失败！");

		return false;
	}

	// 开回显
	Echo(true);

#if NET_DEBUG
	// 获取版本
	GetVersion();
	//auto ver	= GetVersion();
	//net_printf("版本:");
	//ver.Show(true);
#endif

	auto mode = WorkMode;
	// 默认Both
	if (mode == NetworkType::Wire) mode = NetworkType::STA_AP;

	if (GetMode() != mode)
	{
		if (!SetMode(mode))
		{
			net_printf("设置Station模式失败！");

			return false;
		}
		mode = WorkMode;
	}

	Config();

	SetDHCP(mode, true);

	if (!_task) _task = Sys.AddTask(&Esp8266::Process, this, -1, -1, "Esp8266");

	At.Received.Bind(&Esp8266::OnReceive, this);

	return true;
}

bool Esp8266::CheckReady()
{
	// 先关一会电，然后再上电，让它来一个完整的冷启动
	if (!_Power.Empty())
	{
		_Power.Open();					// 使用前必须Open；
		_Power.Down(20);
	}
	if (!_Reset.Empty()) 
	{
		_Reset.Open();		// 使用前必须Open；
		_Reset = true;
	}

	// 如果已连接，不需要再次重启
	if (Test(1, 500)) return true;

	// 每两次启动会有一次打开失败，交替
	if (!_Reset.Empty())
		Reset(false);	// 硬重启
	else
		Reset(true);	// 软件重启命令

	// 如果首次加载，则说明现在处于出厂设置模式，需要对模块恢复出厂设置
	auto cfg = Config::Current->Find("NET");
	//if(!cfg) Restore();

	for (int i = 0; i < 2; i++)
	{
		// 等待模块启动进入就绪状态
		if (!Test(1, 500) && !At.WaitForCmd("ready", 3000))
		{
			if (!Test(10, 1000))
			{
				net_printf("Esp8266::Open 打开失败！");

				return false;
			}
		}
		if (cfg || i == 1) break;

		Restore();
	}

	return true;
}

void Esp8266::OpenAP()
{
	// 未组网时，打开AP，WsLink-xxxxxx
	// 已组网是，STA_AP打开AP，Ws-123456789ABC
	net_printf("启动AP!\r\n");
	String name;
	bool join = SSID && *SSID;
	if (!join || *SSID == "WSWL")
		name = name + "WsLink-" + Buffer(Sys.ID, 3).ToHex();
	else
		// 这里需要等系统配置完成，修改为设备编码
		name = name + "WS-" + Sys.Name;

	int chn = (Sys.Ms() % 14) + 1;
	SetAP(name, "", chn);
#if NET_DEBUG
	//Sys.AddTask(LoadStationTask, this, 10000, 10000, "LoadSTA");
#endif
}

void Esp8266::OnClose()
{
	// 断电
	_Power = false;

	if (_task) Sys.RemoveTask(_task);

	At.Close();

	_Power.Close();
	_Reset.Close();
}

bool Esp8266::OnLink(uint retry)
{
	//if(Linked) return true;

	debug_printf("Esp8266::OnLink\r\n");

	bool join = SSID && *SSID;
	// 等待WiFi自动连接
	if (!At.WaitForCmd("WIFI CONNECTED", 3000))
	{
		auto mode = WorkMode;
		// 默认Both
		if (mode == NetworkType::Wire) mode = NetworkType::STA_AP;
		if (!join || mode == NetworkType::STA_AP) OpenAP();
		if (join)
		{
			if (!JoinAP(*SSID, *Pass)) return false;

			ShowConfig();
			SaveConfig();
		}
	}

	return true;
}

// 配置网络参数
bool Esp8266::Config()
{
	// 设置多连接
	SetMux(true);

	// 设置IPD
	SetIPD(true);

	auto mac = Mac;
	SetMAC(true, mac);
	mac[5]++;
	SetMAC(false, mac);

	//SetAutoConn(AutoConn);

	return true;
}

Socket* Esp8266::CreateSocket(NetType type)
{
	auto es = (EspSocket**)_sockets;

	int i = 0;
	for (i = 0; i < 5; i++)
	{
		if (es[i] == nullptr) break;
	}
	if (i >= 5)
	{
		net_printf("没有空余的Socket可用了 !\r\n");

		return nullptr;
	}

	switch (type)
	{
	case NetType::Tcp:
		return es[i] = new EspTcp(*this, i);

	case NetType::Udp:
		return es[i] = new EspUdp(*this, i);

	default:
		return nullptr;
	}
}

// 启用DNS
bool Esp8266::EnableDNS() { return true; }

// 启用DHCP
bool Esp8266::EnableDHCP()
{
	if (!Opened) return false;

	if (!SetDHCP(NetworkType::STA_AP, true)) return false;

	return true;
}

// 数据到达
void Esp8266::Process()
{
	if (_Buffer.Length() <= 1) return;

	byte idx = _Buffer[0];
	if (idx >= ArrayLength(_sockets)) return;

	// 分发给各个Socket
	auto es = (EspSocket**)_sockets;
	auto sk = es[idx];
	if (sk)
	{
		auto data = _Buffer.Sub(1, -1);
		_Buffer.SetLength(0);

		sk->OnProcess(data, _Remote);
	}

	// 清零长度，其它数据包才可能进来
	_Buffer.SetLength(0);
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
void Esp8266::OnReceive(Buffer& bs)
{
	TS("Esp8266::OnReceive");

	auto str = bs.AsString();

	StringSplit sp(str, ",");

	// 第一段
	auto rs = sp.Next();
	if (!rs) return;
	int idx = rs.ToInt();
	if (idx < 0 || idx > 4) return;

	if (!(rs = sp.Next())) return;
	int len = rs.ToInt();

	IPEndPoint ep;
	if (!(rs = sp.Next())) return;
	ep.Address = IPAddress::Parse(rs);

	// 提前改变下一个分隔符
	sp.Sep = ":";
	if (!(rs = sp.Next())) return;
	ep.Port = rs.ToInt();

	// 后面是数据
	sp.Next();
	int s = sp.Position;
	if (s <= 0) return;

	// 校验数据长度
	int len2 = len;
	int remain = bs.Length() - s;
	if (remain < len2)
	{
		len2 = remain;
		net_printf("剩余数据长度 %d 小于标称长度 %d \r\n", remain, len);
	}

	//sk->OnProcess(bs.Sub(s, len2), ep);
	// 如果有数据包正在处理，则丢弃
	if (_Buffer.Length() > 0)
	{
#if NET_DEBUG
		net_printf("已有数据包 %d 字节正在处理，丢弃当前数据包 %d 字节 \r\n处理：", _Buffer.Length(), len2);
		_Buffer.Show(true);
		net_printf("当前：");
		bs.Sub(s, len2).Show(true);
#endif
	}
	else
	{
		_Remote = ep;
		// 第一字节放Socket索引，数据包放在后面
		_Buffer.SetAt(0, idx);
		_Buffer.Copy(1, bs, s, len2);

		Sys.SetTask(_task, true, 0);
	}
}

/******************************** 基础AT指令 ********************************/

// 基础AT指令
bool Esp8266::Test(int times, int interval)
{
	// 避免在循环内部频繁构造和析构
	String cmd = "AT";
	for (int i = 0; i < times; i++)
	{
		//if (i > 0)	Reset(false);
		if (At.SendCmd(cmd, interval)) return true;
	}

	return false;
}

bool Esp8266::Reset(bool soft)
{
	if (soft) return At.SendCmd("AT+RST");

	_Reset.Down(100);

	return true;
}

/*
AT 版本信息
基于的SDK版本信息
编译生成时间
*/
String Esp8266::GetVersion()
{
	return At.Send("AT+GMR");
}

bool Esp8266::Sleep(uint ms)
{
	String cmd = "AT+GSLP=";
	cmd += ms;
	return At.SendCmd(cmd);
}

bool Esp8266::Echo(bool open)
{
	String cmd = "ATE";
	cmd = cmd + (open ? '1' : '0');
	return At.SendCmd(cmd);
}

// 恢复出厂设置，将擦写所有保存到Flash的参数，恢复为默认参数。会导致模块重启
bool Esp8266::Restore()
{
	return At.SendCmd("AT+RESTORE");
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
bool Esp8266::SetMode(NetworkType mode)
{
	String cmd = "AT+CWMODE=";
	cmd += (byte)mode;

	if (!At.SendCmd(cmd)) return false;

	WorkMode = mode;

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
NetworkType Esp8266::GetMode()
{
	TS("Esp8266::GetMode");

	auto mod = NetworkType::Station;

	auto rs = At.Send("AT+CWMODE?");
	if (!rs) return mod;

	int p = rs.IndexOf(':');
	if (p < 0) return mod;

	return	(NetworkType)rs.Substring(p + 1, 1).ToInt();
}

// +CWJAP:<ssid>,<bssid>,<channel>,<rssi>
// <bssid>目标AP的MAC地址
String Esp8266::GetJoinAP()
{
	return At.Send("AT+CWJAP?");
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

	bool rs = At.SendCmd(cmd, 15000);

#if NET_DEBUG
	net_printf("Esp8266::JoinAP ");
	if (rs)
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
	return At.SendCmd("AT+CWQAP", 2000);
}

/*
开机自动连接WIFI
*/
bool Esp8266::SetAutoConn(bool enable)
{
	String cmd = "AT+CWAUTOCONN=";

	return At.SendCmd(cmd + (enable ? '1' : '0'));
}

// +CWLAP:<enc>,<ssid>,<rssi>,<mac>,<ch>,<freq offset>,<freq calibration>
// freq offset, AP频偏，单位kHz，转为成ppm需要除以2.4
// freq calibration，频偏校准值
String Esp8266::LoadAPs()
{
	return At.Send("AT+CWLAP");
}

// +CWSAP:<ssid>,<pwd>,<chl>,<ecn>,<max conn>,<ssid hidden>
String Esp8266::GetAP()
{
	return At.Send("AT+CWSAP");
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

	return At.SendCmd(cmd, 1600);
}

// <ip addr>,<mac>
// 查询连接到AP的Stations信息。无法查询DHCP接入
String Esp8266::LoadStations()
{
	return At.Send("AT+CWLIF");
}

bool Esp8266::GetDHCP(bool* sta, bool* ap)
{
	auto rs = At.Send("AT+CWDHCP?");
	if (!rs) return false;

	byte n = rs.ToInt();
	*sta = n & 0x01;
	*ap = n & 0x02;

	return true;
}

bool Esp8266::SetDHCP(NetworkType mode, bool enable)
{
	byte m = 0;
	switch (mode)
	{
	case NetworkType::Station:
		m = 1;
		break;
	case NetworkType::AP:
		m = 0;
		break;
	case NetworkType::STA_AP:
		m = 2;
		break;
	default:
		return false;
	}

	String cmd = "AT+CWDHCP=";
	String n = enable ? "1" : "0";

	return At.SendCmd(cmd + m + ',' + n);
}

MacAddress Esp8266::GetMAC(bool sta)
{
	auto rs = At.Send(sta ? "AT+CIPSTAMAC?" : "AT+CIPAPMAC?");
	int p = rs.IndexOf(':');
	if (p < 0) return MacAddress::Empty();

	return MacAddress::Parse(rs.Substring(p + 1, -1));
}

// 设置MAC会导致WiFi连接断开
bool Esp8266::SetMAC(bool sta, const MacAddress& mac)
{
	String cmd = sta ? "AT+CIPSTAMAC" : "AT+CIPAPMAC";
	cmd = cmd + "=\"" + mac.ToString().Replace('-', ':') + '\"';

	return At.SendCmd(cmd);
}

IPAddress Esp8266::GetIP(bool sta)
{
	auto rs = At.Send(sta ? "AT+CIPSTA?" : "AT+CIPAP?");
	int p = rs.IndexOf("ip:\"");
	if (p < 0) return IPAddress::Any();

	p += 4;
	int e = rs.IndexOf("\"", p);
	if (e < 0) return IPAddress::Any();

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
	return At.Send("AT+CIPSTATUS?");
}

bool Esp8266::GetMux()
{
	auto rs = At.Send("AT+CIPMUX?");
	int p = rs.IndexOf(':');
	if (p < 0) return false;

	return rs.Substring(p + 1, 1) != "0";
}

bool Esp8266::SetMux(bool enable)
{
	String cmd = "AT+CIPMUX=";
	return At.SendCmd(cmd + (enable ? '1' : '0'));
}

bool Esp8266::Update()
{
	return At.SendCmd("AT+CIPUPDATE");
}

bool Esp8266::Ping(const IPAddress& ip)
{
	String cmd = "AT+PING=";

	return At.SendCmd(cmd + ip);
}

bool Esp8266::SetIPD(bool enable)
{
	String cmd = "AT+CIPDINFO=";
	return At.SendCmd(cmd + (enable ? '1' : '0'));
}

void LoadStationTask(void* param)
{
	auto& esp = *(Esp8266*)param;
	if (esp.Opened) esp.LoadStations();
}

/******************************** Invoke指令 ********************************/
// 设置无线组网密码。匹配令牌协议
bool Esp8266::SetWiFi(const Pair& args, Stream& result)
{
	String ssid;
	String pass;

	if (!args.Get("ssid", ssid)) return false;
	if (!args.Get("pass", pass))	// 获取失败  直接给0的长度
	{
		pass.Clear();
		pass.SetLength(0);
		// return false;
	}

	bool haveChang = false;
	// 现有密码与设置密码不一致才写FLASH
	if (*SSID != ssid || *Pass != pass)
	{
		haveChang = true;
		// 保存密码
		*SSID = ssid;
		*Pass = pass;
		// 组网后单独STA模式，调试时使用混合模式
		//#if DEBUG
		//	Mode	= NetworkType::STA_AP;
		//#else
		Mode = NetworkType::Station;
		//#endif
		SaveConfig();
	}

	// Sleep跳出去处理其他的
	// 让JoinAp相关的东西数据先处理掉  然后再去回复  较少处理数据冲突的情况。
	Sys.Sleep(150);

	// 返回结果
	result.Write((byte)true);
	// 延迟重启
	if (haveChang) Sys.Reboot(5000);

	return true;
}
// 获取WIFI名称
bool Esp8266::GetWiFi(const Pair& args, Stream& result)
{
	// 返回结果
	result.Write(*SSID);

	return true;
}

void Esp8266::GetAPsTask()
{
	if (APs == nullptr)APs = new String();
	APs->Clear();
	*APs = LoadAPs();
}

bool Esp8266::GetAPs(const Pair& args, Stream& result)
{
	bool rt = false;

	if (APs && APs->Length())
	{
		Buffer buf((void*)APs->GetBuffer(), APs->Length());
		result.Write(buf);
		APs->Clear();
		// delete APs;
		rt = true;
	}
	else
	{
		result.Write((byte)false);
		rt = false;
	}

	// 只运行一次
	Sys.AddTask(&Esp8266::GetAPsTask, this, 0, -1, "GetAPs");

	return rt;
}

