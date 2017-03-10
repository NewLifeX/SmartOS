#include "Kernel\Sys.h"
#include "Kernel\Task.h"
#include "Kernel\TTime.h"
#include "Kernel\WaitHandle.h"

#include "Device\SerialPort.h"

#include "GSM07.h"

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
	String* Command = nullptr;
	String* Result = nullptr;
	cstring	Key1 = nullptr;
	cstring	Key2 = nullptr;

	bool	Capture = true;	// 是否捕获所有
};

/*
		注意事项
1、设置模式AT+CWMODE需要重启后生效AT+RST
2、AP模式下查询本机IP无效，可能会造成死机
3、开启server需要多连接作为基础AT+CIPMUX=1
4、单连接模式，多连接模式  收发数据有参数个数区别
*/

/******************************** GSM07 ********************************/

GSM07::GSM07()
{
	Name = "GSM07";
	Speed = 10;

	//APN = "CMNET";
	APN = nullptr;
	Mux = true;

	Led = nullptr;

	//Country = 0;
	Network = 0;
	Area = 0;
	CellID = 0;

	Buffer(_sockets, 5 * 4).Clear();

	Mode = NetworkType::STA_AP;

	At.DataKey = "+CIPRCV:";

	InitConfig();
	LoadConfig();
}

GSM07::~GSM07()
{
	RemoveLed();
}

void GSM07::Init(COM idx, int baudrate)
{
	At.Init(idx, baudrate);
}

void GSM07::Init(ITransport* port)
{
	At.Init(port);
}

void GSM07::Set(Pin power, Pin rst, Pin low)
{
	if (power != P0) _Power.Set(power);
	if (rst != P0) _Reset.Set(rst);
	if (low != P0) _LowPower.Set(low);
}

void GSM07::SetLed(Pin led)
{
	if (led != P0)
	{
		auto port = new OutputPort(led);
		SetLed(*port);
	}
}

void GSM07::SetLed(OutputPort& led)
{
	auto fp = new FlushPort();
	fp->Port = &led;
	fp->Start();
	Led = fp;
}

void GSM07::RemoveLed()
{
	if (Led)delete (FlushPort*)Led;
	Led = nullptr;
}

bool GSM07::OnOpen()
{
	if (!At.Open()) return false;

	// 回显
	Echo(true);

	// 先检测AT失败再重启。保证模块处于启动状态，降低网络注册时间损耗
	//if (!Test(1, 1000) && !CheckReady())
	if (!CheckReady())
	{
		net_printf("GSM07::Open 打开失败！");

		return false;
	}

#if NET_DEBUG
	// 获取版本
	GetVersion();
	/*auto ver = GetVersion();
	net_printf("版本:");
	ver.Show(true);*/
	GetCCID();
	//GetMobiles();
	GetMobile();
	GetIMEI();
#endif
	GetIMSI();

	// 检查网络质量
	//QuerySignal();
	/*if (!QueryRegister())
	{
		net_printf("GSM07::Open 注册网络失败，打开失败！");

		return false;
	}*/

	Config();

	At.Received.Bind(&GSM07::OnReceive, this);

	return true;
}

bool GSM07::CheckReady()
{
	// 先关一会电，然后再上电，让它来一个完整的冷启动
	if (!_Power.Empty())
	{
		_Power.Open();					// 使用前必须Open
		_Power.Down(20);
	}
	if (!_Reset.Empty()) _Reset.Open();		// 使用前必须Open

	// 每两次启动会有一次打开失败，交替
	if (!_Reset.Empty())
		Reset(false);	// 硬重启
	else
		Reset(true);	// 软件重启命令

	// 等待模块启动进入就绪状态
	if (!Test())
	{
		net_printf("GSM07::Open 打开失败！");

		return false;
	}

	return true;
}

void GSM07::OnClose()
{
	// 先断开已有连接
	At.SendCmd("AT+CIPSHUT\r");
	// Device will be switched off (power down mode). Do not send any command after this command.
	At.SendCmd("AT+CPOF\r");

	At.Close();

	_Power.Close();
	_Reset.Close();
}

bool GSM07::OnLink(uint retry)
{
	//if(Linked) return true;

	debug_printf("GSM07::OnLink\r\n");

	//if (!QueryRegister()) return false;

	return Config();

	//return true;
}

// 配置网络参数
bool GSM07::Config()
{
	/*
	GPRS透传
	AT+CGATT=1 Attach to the GPRS network, can also use parameter 0 to detach.
	OK Response, attach successful
	AT+CGDCONT=? Input test command for help information.
	+CGDCONT: (1..7), (IP,IPV6,PPP),(0..3),(0..4)
	OK Response, show the helpful information.
	AT+CGDCONT=1, "IP", "cmnet" Before active, use this command to set PDP context.
	OK Response. Set context OK.
	AT+CGACT=1,1 Active command is used to active the specified PDP context.
	OK Response, active successful.
	ATD*99***1# This command is to start PPP translation.
	CONNECT Response, when get this, the module has been set to data state. PPP data should be transferred after this response and anything input is treated as data.
	+++ This command is to change the status to online data state. Notice that before input this command, you need to wait for a three seconds’ break, and it should also be followed by 3 seconds’ break, otherwise “+++” will be treated as data.
	*/

	/*
	TCP/IP 数据操作
	at+cipstatus Check the status of TCP/IP +IPSTATUS: IP INITIAL
	OK Response, in the state of INITIAL

	AT+CIPSTART="TCP","124.42.0.80",7 Start TCP/IP, if the MS hadn’t attached to the GPRS network, this command will fulfill all the prepare task and make ready for TCP/IP data transfer.
	CONNECT OK
	OK Response

	at+cipstatus Check the status of TCP/IP
	+IPSTATUS: CONNECT OK
	OK Response, in the state of CONNECT

	at+cipsend
	> this is a test<ctl+z> Send data “this is a test” ended with ctrl+z
	OK Response

	at+cifsr Check IP
	10.8.18.69 OK Response

	at+cipclose Close a TCP/IP translation
	OK Response

	at+cipstatus Check status
	+IPSTATUS: IP CLOSE
	OK In the state of IP CLOSE

	AT+CIPSHUT Disconnect the wireless connection
	OK

	at+cipstatus Check status
	+IPSTATUS: IP INITIAL
	OK Return to the initial status
	*/

	//IPShutdown(0);

	// 检查网络质量
	QuerySignal();
	//QueryRegister();

	// 附着网络，上网必须
	if (!AttachMT(true)) return false;
	// 执行网络查询可能导致模块繁忙
	//if (!QueryRegister()) return false;

	if (!SetAPN(APN, false)
		|| !SetPDP(true)
		|| !SetClass("B")) return false;

	IPMux(Mux);
	IPStatus();

	return true;
}

Socket* GSM07::CreateSocket(NetType type)
{
	/*auto es = (EspSocket**)_sockets;

	int i = 0;
	for (i = 0; i < 5; i++)
	{
		if (es[i] == nullptr) break;
	}
	if (i >= 5)
	{
		net_printf("没有空余的Socket可用了 !\r\n");

		return nullptr;
	}*/

	switch (type)
	{
		/*case NetType::Tcp:
			return es[i] = new EspTcp(*this, i);

		case NetType::Udp:
			return es[i] = new EspUdp(*this, i);*/

	default:
		return nullptr;
	}
}

// 数据到达
void GSM07::OnReceive(Buffer& bs)
{
	Received(bs);
}

/******************************** 基础AT指令 ********************************/

// 基础AT指令
bool GSM07::Test(int times, int interval)
{
	// 避免在循环内部频繁构造和析构
	String cmd = "AT";
	for (int i = 0; i < times; i++)
	{
		if (i > 0)	Reset(false);
		if (At.SendCmd(cmd, interval)) return true;
	}

	return false;
}

bool GSM07::Reset(bool soft)
{
	if (soft) return At.SendCmd("AT+RST");

	_Reset.Up(100);
	return true;
}

// AT 版本信息
String GSM07::GetVersion()
{
	//return At.Send("AT+GMR");
	//return At.Send("AT+GSN");
	return At.Send("ATI");
}

bool GSM07::Sleep(uint ms)
{
	String cmd = "AT+GSLP=";
	cmd += ms;
	return At.SendCmd(cmd);
}

bool GSM07::Echo(bool open)
{
	String cmd = "ATE";
	cmd = cmd + (open ? '1' : '0');
	return At.SendCmd(cmd);
}

// 恢复出厂设置，将擦写所有保存到Flash的参数，恢复为默认参数。会导致模块重启
bool GSM07::Restore() { return At.SendCmd("AT+RESTORE"); }

String GSM07::GetIMSI()
{
	auto rs = At.Send("AT+CIMI");
	if (rs.Length() == 0) return rs;

	// 460040492206250
	//Country = rs.Substring(0, 3).ToInt();
	//Network = rs.Substring(3, 2).ToInt();
	Network = (uint)rs.TrimStart().Substring(0, 5).ToInt();

	// 自动设置APN
	if (!APN)
	{
		switch (Network)
		{
			// 中国移动
		case 46000:
		case 46002:
		case 46004:
		case 46007:
			APN = "CMNET";
			break;
			// 中国联通
		case 46001:
			APN = "UNINET";
			break;
			// 中国电信
		case 46003:
			APN = "CTNET";
			break;
			// 香港电讯
		case 45400:
		case 45402:
		case 45418:
			APN = "CHKT";
			break;
			// 香港万众电话
		case 45412:
		case 45413:
			APN = "CMHK";
			break;
		default:
			break;
		}

		if (APN) debug_printf("GSM07::GetMobile 根据IMSI自动设置APN=%s \r\n", APN);
	}

	return rs;
}

String GSM07::GetIMEI() { return At.Send("AT+EGMR=2,7"); }
// 查询SIM的CCID，也可以用于查询SIM是否存或者插好
String GSM07::GetCCID() { return At.Send("AT+CCID"); }

/******************************** 网络服务 ********************************/
// 获取运营商名称。非常慢
String GSM07::GetMobiles() { return At.Send("AT+COPN"); }

String GSM07::GetMobile()
{
	/*
	mode + format
	mode = 0 表示自动选择网络

	format
	0 长格式名称 如 ChinaMobile
	1 短格式名称 如 CMCC
	2 数字 如 46000

	修改后貌似要重启才能生效

	可以根据这个自动设置APN
	*/
	//At.SendCmd("AT+COPS=0,0");

	auto rs = At.Send("AT+COPS?");

	// 自动设置APN
	/*if (!APN)
	{
		if (rs.Contains("46000") || rs.Contains("CMCC") || rs.Contains("ChinaMobile"))
			APN = "cmnet";

		if (APN)debug_printf("GSM07::GetMobile 根据网络运营商自动设置APN=%s \r\n", APN);
	}*/

	return rs;
}

bool GSM07::QueryRegister()
{
	At.SendCmd("AT+CREG=2");

	/*
	类型，状态，本地区域，CellID
	+CREG: 2,5,"26F4","BEF8"

	0 not registered, MT is not currently searching a new operator to register to
	1 registered, home network
	2 not registered, but MT is currently searching a new operator to register to
	3 registration denied
	4 unknown
	5 registered, roaming
	*/
	auto rs = At.Send("AT+CREG?", "OK");
	//if (!rs.StartsWith("+CREG: ")) return false;

	// 去掉头部，然后分割
	//rs = rs.Substring(7);
	// 直接分割，反正不要第一段
	auto sp = rs.Split(",");

	// 跳过第一段
	sp.Next();
	int state = sp.Next().ToInt();
	Area = sp.Next().Substring(1, 4).ToHex().ToUInt16();
	CellID = sp.Next().Substring(1, 4).ToHex().ToUInt16();

	return state == 1 || state == 5;
}

String GSM07::QuerySignal() { return At.Send("AT+CSQ"); }

bool GSM07::AttachMT(bool enable)
{
	if (enable)
		return At.SendCmd("AT+CGATT=1");
	else
		return At.SendCmd("AT+CGATT=0");
}

// 00:CMNET 10:CMHK 01:CHKT 11:HKCSL
bool GSM07::SetAPN(cstring apn, bool issgp)
{
	if (!apn) apn = "CMNET";

	String cmd;
	if (issgp)
		cmd = "AT+CIPCSGP=1";
	else
		cmd = "AT+CGDCONT=1,\"IP\"";
	cmd = cmd + ",\"" + apn + "\"";

	return At.SendCmd(cmd);
}

bool GSM07::SetPDP(bool enable)
{
	if (enable)
		return At.SendCmd("AT+CGACT=1");
	else
		return At.SendCmd("AT+CGACT=0");
}

IPAddress GSM07::GetIP()
{
	auto rs = At.Send("AT+CIMI");
	rs.Show(true);

	rs = At.Send("AT+CIFSR");
	rs.Show(true);

	int p = rs.IndexOf("\r\n");
	int q = rs.IndexOf("\r\n", p + 1);
	rs = rs.Substring(p, q - p - 2);

	return IPAddress::Parse(rs);
}

bool GSM07::SetClass(cstring mode)
{
	if (!mode)mode = "B";

	String cmd = "AT+CGCLASS=\"";
	cmd += mode;
	cmd += "\"";

	return At.SendCmd(cmd);
}

/******************************** TCP/IP ********************************/
int GSM07::IPStart(const NetUri& remote)
{
	String cmd = "AT+CIPSTART=\"";
	switch (remote.Type)
	{
	case NetType::Tcp:
	case NetType::Http:
		cmd += "TCP";
		break;
	case NetType::Udp:
		cmd += "UDP";
		break;
	default:
		return -1;
	}
	//cmd = cmd + "\",\"" + remote.Address + "\",\"" + remote.Port + "\"";
	cmd = cmd + "\",\"" + remote.Address + "\"," + remote.Port;

	if (!Mux) return At.SendCmd(cmd) ? 1 : -1;

	// +CIPNUM:0 截取链路号
	auto rs = At.Send(cmd, "OK");
	int p = rs.IndexOf(":");
	if (p < 0)return -1;
	int q = rs.IndexOf("\r");

	return rs.Substring(p + 1, q - p - 1).ToInt();
}

bool GSM07::SendData(const String& cmd, const Buffer& bs)
{
	// 重发3次AT指令，避免busy
	int i = 0;
	for (i = 0; i < 3; i++)
	{
		//auto rt	= _Host.Send(cmd, ">", "OK", 1600);
		// 不能等待OK，而应该等待>，因为发送期间可能给别的指令碰撞
		auto rt = At.Send(cmd, ">", "ERROR", 1600);
		if (rt.Contains(">")) break;

		Sys.Sleep(500);
	}
	if (i < 3 && At.Send(bs.AsString(), "SEND OK", "ERROR", 1600).Contains("SEND OK"))
	{
		return true;
	}

	/*// 发送失败，关闭链接，下一次重新打开
	if (++_Error >= 10)
	{
		_Error = 0;

		Close();
	}*/

	return false;
}

bool GSM07::IPSend(int index, const Buffer& data)
{
	assert(data.Length() <= 1024, "每次最多发送1024字节");

	/*
	AT+CIPSEND=5,”12345” //同步发送字符串
	AT+CIPSEND=5 //出现”>”后可以发送5个字节的二进制数据
	AT+CIPSEND //出现”>”后可以发送以CTRL+Z结尾的字符串
	*/

	String cmd = "AT+CIPSEND=";
	if (Mux) cmd = cmd + index + ",";
	cmd = cmd + data.Length() + "\r\n";

	return SendData(cmd, data);
}

bool GSM07::IPClose(int index)
{
	String cmd = "AT+CIPSEND=";
	if (Mux) cmd = cmd + index + ",";

	return At.SendCmd("AT+CIPCLOSE\r\n");
}

bool GSM07::IPShutdown(int index)
{
	return At.SendCmd("AT+CIPSHUT\r\n");
}

String GSM07::IPStatus() { return At.Send("AT+CIPSTATUS"); }

bool GSM07::SetAutoSendTimer(bool enable, ushort time)
{
	String cmd = "AT+CIPATS=";
	cmd = cmd + (enable ? "1" : "0") + "," + time;

	return At.SendCmd(cmd);
}

IPAddress GSM07::DNSGetIP(const String& domain)
{
	String cmd = "AT+CDNSGIP=";
	cmd += domain;

	auto rs = At.Send(cmd, "OK");
	rs.Show(true);

	int p = rs.IndexOf("\r\n");
	int q = rs.IndexOf("\r\n", p + 1);
	rs = rs.Substring(p, q - p - 2);

	return IPAddress::Parse(rs);
}

bool GSM07::IPMux(bool enable)
{
	String cmd = "AT+CIPMUX=";
	cmd = cmd + (enable ? "1" : "0");

	return At.SendCmd(cmd);
}

bool GSM07::IPHeartConfig(int index, int mode, int value)
{
	/*
	AT+CIPHCFG=?可以查询该指令的用法。
	AT+CIPHCFG=mode,param;
	参数说明：
	Mode: 0, 心跳包间隔时间，单位秒，参数为5-7200
	1， 心跳发送包，长度不超过100个字节
	2， 回应包，长度不超过100个字节
	AT+CIPHCFG?查询配置的参数
	AT+CIPHCFG=0,15,配置15秒发送一次心跳包
	AT+CIPHCFG=1,553435ff,配置心跳包的内容为16进制的” 553435ff”
	AT+CIPHCFG=2,883435ee, 配置回应心跳包的内容为16进制的” 883435ee”
	*/

	String cmd = "AT+CIPHCFG=";
	if (Mux) cmd = cmd + index + ",";
	cmd = cmd + mode + ",";

	auto cs = (cstring)value;

	if (mode == 0) return At.SendCmd(cmd + value);
	if (mode == 1) return At.SendCmd(cmd + cs);
	if (mode == 2) return At.SendCmd(cmd + cs);

	return false;
}

bool GSM07::IPHeart(int index, bool enable)
{
	String cmd = "AT+CIPHMODE=";
	if (Mux) cmd = cmd + index + ",";
	cmd = cmd + (enable ? "1" : "0");

	return At.SendCmd(cmd);
}

// 透明传输
bool GSM07::IPTransparentConfig(int mode, int value)
{
	/*
	AT+CIPTCFG=?可以查询该指令的用法。
	AT+CIPTCFG=mode,param;
	参数说明：
	Mode: 0, 失败发送次数，参数为0-5次；
	1， 失败发送延时，参数0-3000毫秒；
	2， 触发发送的发送包大小，取值为10-100，；当发送内容达到这个长度，立马启动发送；
	3， 触发发送的延时，1000-8000，毫秒，当向串口发送的最后一个字符完成后，延时这个时间就可以触发发送；
	AT+CIPTCFG？查询配置的参数
	AT+CIPTCFG=0,15,配置15秒发送一次心跳包
	*/

	String cmd = "AT+CIPTCFG=";
	cmd = cmd + mode + ",";

	return At.SendCmd(cmd + value);
}

bool GSM07::IPTransparent(bool enable)
{
	if (enable)
		return At.SendCmd("AT+CIPTCFG=1");
	else
		return At.SendCmd("+++");
}
