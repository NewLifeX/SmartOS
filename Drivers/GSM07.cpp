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

/******************************** GSM07 ********************************/

GSM07::GSM07()
{
	Name = "GSM07";
	Speed = 10;

	//APN = "CMNET";
	APN = nullptr;
	Mux = false;

	Led = nullptr;

	//Country = 0;
	Network = 0;
	Area = 0;
	CellID = 0;

	Buffer(Sockets, 5 * 4).Clear();

	Mode = NetworkType::STA_AP;

	//At.DataKey = "+CIPRCV:";

	_task = 0;

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
	/*_Power.Invert = 1;
	_Power.OpenDrain = true;
	_Power.Open();					// 使用前必须Open
	_Power.Up(500);
	//_Power.Write(false);			// 打开电源（低电平有效）
	_Power.Close();*/
	_Power.Invert = 0;
	_Power.Open();					// 使用前必须Open
	_Power.Write(false);			// 打开电源（低电平有效）
	debug_printf("SIM800C::OnOpen Power=%d \r\n", _Power.ReadInput());

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
	
	GPSGpioOut();
	GPSPowerOpen();
	Sys.Sleep(1);
	NameExplain();
	WaitForGPSSuccess();
	GPSSetTime();
	//GPSMessageOut();

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

	// 这里不用配置，待会OnLink马上就会执行
	//Config();

	// 接收数据时是否增加IP头提示
	At.SendCmd("AT+CIPHEAD=1");

	if (!_task) _task = Sys.AddTask(&GSM07::Process, this, -1, -1, "GSM07");

	At.Received.Bind<GSM07>([](GSM07& gsm, Buffer& bs) { gsm.OnReceive(bs); }, this);

	return true;
}

bool GSM07::CheckReady()
{
	// 先关一会电，然后再上电，让它来一个完整的冷启动
	if (!_Power.Empty())
	{
		//_Power.Up(500);
		//_Power.Write(false);
		net_printf("SIM800C::CheckReady  Power=%d \r\n", _Power.ReadInput());
	}
	if (!_Reset.Empty()) _Reset.Open();		// 使用前必须Open

	Reset(true);	// 软件重启命令
	if (!_Reset.Empty()) Reset(false);	// 硬重启

	// 等待模块启动进入就绪状态
	if (!Test(40, 500))
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

	// 1，检查网络质量
	QuerySignal();
	//QueryRegister();

	// 2，网络查询
	if (!QueryRegister()) return false;
	//QueryRegister();

	if (!SetClass("B")) return false;

	SetAPN(APN, false);

	// 3，附着网络
	if (!AttachMT(true)) return false;

	// 4，设置PDP参数
	SetAPN(APN, true);

	// 5，激活PDP
	SetPDP(true);

	if (Mux) Mux = IPMux(true);
	IPStatus();
	At.SendCmd("AT+CDNSCFG?");
	At.SendCmd("AT+CDNSCFG=\"180.76.76.76\",\"223.5.5.5\"");
	At.SendCmd("AT+CDNSGIP=\"smart.wslink.cn\"");
	At.SendCmd("AT+CIPHEAD=1");
	return true;
}

Socket* GSM07::CreateSocket(NetType type)
{
	auto es = (GSMSocket**)Sockets;

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
		return es[i] = new GSMTcp(*this, i);

	case NetType::Udp:
		return es[i] = new GSMUdp(*this, i);

	default:
		return nullptr;
	}
}

// 数据到达
void GSM07::Process()
{
	if (_Buffer.Length() <= 1) return;

	byte idx = _Buffer[0];
	if (idx >= ArrayLength(Sockets)) return;

	// 分发给各个Socket
	auto es = (GSMSocket**)Sockets;
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

// 数据到达
void GSM07::OnReceive(Buffer& bs)
{
	OnProcess(0, bs, _Remote);
}

void GSM07::OnProcess(int index, Buffer& data, const IPEndPoint& remote)
{
	Received(data);

	// 如果有数据包正在处理，则丢弃
	if (_Buffer.Length() > 0)
	{
#if NET_DEBUG
		net_printf("已有数据包 %d 字节正在处理，丢弃当前数据包 %d 字节 \r\n处理：", _Buffer.Length(), data.Length());
		_Buffer.Show(true);
		net_printf("当前：");
		data.Show(true);
#endif
	}
	else
	{
		_Remote = remote;
		// 第一字节放Socket索引，数据包放在后面
		_Buffer.SetAt(0, index);
		_Buffer.Copy(1, data, 0, -1);

		Sys.SetTask(_task, true, 0);
	}
}

/******************************** 基础AT指令 ********************************/

// 基础AT指令
bool GSM07::Test(int times, int interval)
{
	// 避免在循环内部频繁构造和析构
	String cmd = "AT";
	for (int i = 0; i < times; i++)
	{
		//if (i > 0)	Reset(false);
		Sys.Sleep(500);
		if (At.SendCmd(cmd, interval)) return true;
	}

	return false;
}

bool GSM07::Reset(bool soft)
{
	if (soft) return At.SendCmd("AT+RST");

	/*
	模块硬件RESET脚，此脚使用的时候低电平<0.05V,电流在70ma左右，必须使用NMOS可以控制；
	拉高以后其实是模块硬件关机了，该脚在正常工作的时候不能有漏电，否则会导致模块不稳定，难以注册网络；
	在RESET的时候注意PWR_KEY脚要先拉低，然后再拉高。
	*/
	if (!_Power.Empty()) _Power.Up(500);
	_Reset.Up(100);

	return true;
}

// AT 版本信息
String GSM07::GetVersion()
{
	//return At.Send("AT+GMR");
	//return At.Send("AT+GSN");
	auto rs = At.Send("ATI");
	At.Send("AT+CGMI");
	At.Send("AT+CGMM");

	return rs;
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
String GSM07::GetCCID() {
	At.Send("AT+CPIN?");
	At.Send("AT+CNUM");
	return At.Send("AT+CCID");
}

/******************************** GPS服务 ********************************/

//设置GPIO1输出高电平
bool GSM07::GPSGpioOut()
{
	return At.SendCmd("AT+CGPIO=0,57,1,1");
}
//打开电源
bool GSM07::GPSPowerOpen()
{
	return At.SendCmd("AT+CGNSPWR=1");
}

//名字解析
bool GSM07::NameExplain()
{
	return At.SendCmd("AT+CGNSSEQ=\"RMC\"");
}

//等待GPS开机
bool GSM07::WaitForGPSSuccess()
{
	auto rs = At.Send("AT+CGNSINF",5000);
	return true;
}

//设置GPS数据输出时间
bool GSM07::GPSSetTime()
{
	return At.SendCmd("AT+CGNSURC=3");
}

//打开GPS数据流输出
bool GSM07::GPSMessageOut()
{
	return At.SendCmd("AT+CGNSTST=1");
}



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
	auto rs = At.Send("AT+CREG?", 5000);
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
		return At.SendCmd("AT+CGACT=1,1");
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
	if (remote.Host)
		cmd = cmd + "\",\"" + remote.Host + "\"," + remote.Port;
	else
		cmd = cmd + "\",\"" + remote.Address + "\"," + remote.Port;

	//if (!Mux) return At.SendCmd(cmd) ? 1 : -1;

	// +CIPNUM:0 截取链路号
	auto rs = At.Send(cmd + "\r\n", "CONNECT", "BIND", 10000, false);

	// 有可能没有打开多链接
	if (!Mux) return rs.Length() == 0 || rs.Contains("FAIL") || rs.Contains("ERROR") ? -1 : 0;

	int p = rs.IndexOf(":");
	if (p < 0) return -2;
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
		auto rt = At.Send(cmd, ">", "ERROR", 2000, false);
		if (rt.Contains(">")) break;

		Sys.Sleep(500);
	}

	
	
	if (i < 3 && At.Send(bs.AsString(), "SEND OK", "ERROR", 1600).Contains("SEND OK"))
	{
		return true;
	}
	
	if (++_Error >= 3)
	{
		_Error = 0;

		Close();
		net_printf("发送超过三次 关闭链接 下一次重新打开！\r\n");
	}
	if (_Error == 0)
	{
		Sys.Reboot(500);
		net_printf(" SmartOS将在500毫秒后重启\r\n");
	}
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

	// 数据较短时，直接发送
	if (data.Length() < 256)
	{
		// 字符串里面不能有特殊字符
		bool flag = true;
		for (int i = 0; i < data.Length(); i++)
		{
			if (data[i] == '\0' || data[i] == '\"')
			{
				flag = false;
				break;
			}
		}
		if (flag)
		{
			cmd = cmd + data.Length() + ",\"" + data.AsString() + "\"";
			return At.SendCmd(cmd, 1600);
		}
	}

	cmd = cmd + data.Length() + "\r\n";

	return SendData(cmd, data);
}

bool GSM07::IPClose(int index)
{
	String cmd = "AT+CIPCLOSE=";
	if (Mux)
		cmd = cmd + index + "[1]";
	else
		cmd += 1;

	bool rs = At.SendCmd(cmd, 3000);
	
	if (!Mux) At.SendCmd("AT+CIPSHUT", 10000);

	return rs;
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

	auto rs = At.Send(cmd);
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

/******************************** Socket ********************************/

GSMSocket::GSMSocket(GSM07& host, NetType protocol, byte idx)
	: _Host(host)
{
	_Index = idx;
	_Error = 0;

	Host = &host;
	Protocol = protocol;
}

GSMSocket::~GSMSocket()
{
	if (_Host.Sockets[_Index] == this) _Host.Sockets[_Index] = nullptr;

	Close();
}

bool GSMSocket::OnOpen()
{
	// 确保宿主打开
	if (!_Host.Open()) return false;

	// 如果没有指定本地端口，则使用累加端口
	if (!Local.Port)
	{
		// 累加端口
		static ushort g_port = 1024;
		// 随机拿到1024 至 1024+255 的端口号
		if (g_port <= 1024)g_port += Sys.Ms() & 0xff;

		Local.Port = g_port++;
	}
	Local.Address = _Host.IP;

	//_Host.SetMux(true);

#if NET_DEBUG
	net_printf("%s::Open ", Protocol == NetType::Tcp ? "Tcp" : "Udp");
	Local.Show(false);
	net_printf(" => ");
	if (Server)
	{
		Server.Show(false);
		net_printf(":%d\r\n", Remote.Port);
	}
	else
		Remote.Show(true);
#endif

	NetUri uri;
	uri.Type = Protocol;
	uri.Address = Remote.Address;
	uri.Port = Remote.Port;
	uri.Host = Server;

#if NET_DEBUG
	//_Host.IPStatus();
#endif

	// 先关闭一次
	OnClose();

	int idx = _Host.IPStart(uri);
	if (idx < 0)
	{
		net_printf("协议 %d, %d 打开失败 %d \r\n", (byte)Protocol, Remote.Port, idx);
		return false;
	}
	_Index = idx;

	_Error = 0;

	return true;
}

void GSMSocket::OnClose()
{
	_Host.IPClose(_Index);
}

// 接收数据
uint GSMSocket::Receive(Buffer& bs)
{
	if (!Open()) return 0;

	return 0;
}

// 发送数据
bool GSMSocket::Send(const Buffer& bs)
{
	if (!Open()) return false;

	return _Host.IPSend(_Index, bs);
}

bool GSMSocket::OnWrite(const Buffer& bs) { return Send(bs); }
uint GSMSocket::OnRead(Buffer& bs) { return Receive(bs); }

// 收到数据
void GSMSocket::OnProcess(const Buffer& bs, const IPEndPoint& remote)
{
	OnReceive((Buffer&)bs, (void*)&remote);
}

/******************************** Tcp ********************************/

GSMTcp::GSMTcp(GSM07& host, byte idx)
	: GSMSocket(host, NetType::Tcp, idx)
{

}

/******************************** Udp ********************************/

GSMUdp::GSMUdp(GSM07& host, byte idx)
	: GSMSocket(host, NetType::Udp, idx)
{

}

bool GSMUdp::SendTo(const Buffer& bs, const IPEndPoint& remote)
{
	//!!! ESP8266有BUG，收到数据后，远程地址还是乱了，所以这里的远程地址跟实际可能不一致
	if (remote == Remote) return Send(bs);

	if (!Open()) return false;

	//remote.Show(true);
	//assert(false, "不支持改变UDP远程地址");

	//return false;
	return Send(bs);
}

bool GSMUdp::OnWriteEx(const Buffer& bs, const void* opt)
{
	auto ep = (IPEndPoint*)opt;
	if (!ep) return OnWrite(bs);

	return SendTo(bs, *ep);
}
