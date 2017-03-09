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

	APN = "CMNET";

	Led = nullptr;

	Buffer(_sockets, 5 * 4).Clear();

	Mode = NetworkType::STA_AP;

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

	if (!CheckReady())
	{
		net_printf("GSM07::Open 打开失败！");

		return false;
	}

	At.Received.Bind(&GSM07::OnReceive, this);

	// 开回显
	Echo(true);

#if NET_DEBUG
	// 获取版本
	//GetVersion();
	auto ver	= GetVersion();
	net_printf("版本:");
	ver.Show(true);
#endif

	Config();

	return true;
}

bool GSM07::CheckReady()
{
	// 先关一会电，然后再上电，让它来一个完整的冷启动
	if (!_Power.Empty())
	{
		_Power.Open();					// 使用前必须Open；
		_Power.Down(20);
	}
	if (!_Reset.Empty()) _Reset.Open();		// 使用前必须Open；

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

	/*bool join = SSID && *SSID;
	// 等待WiFi自动连接
	if (!WaitForCmd("WIFI CONNECTED", 3000))
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
	}*/

	return true;
}

// 配置网络参数
void GSM07::Config()
{
	Echo(true);
	At.SendCmd("AT+CIPSHUT\r");
	At.SendCmd("AT+CGCLASS=\"B\"\r");
	SetAPN(APN, false);
	At.SendCmd("AT+CGATT=1\r");
	// 先断开已有连接
	//At.SendCmd("AT+CIPSHUT\r");
	//设置APN
	SetAPN(APN, true);
	At.SendCmd("AT+CLPORT=\"UDP\",\"3399\"\r");
	// IP设置方式
	//At.SendCmd("AT+CIPSTART=\"UDP\",\"183.63.213.113\",\"3388\"\r");
	// 域名设置方式
	At.SendCmd("AT+CIPMUX=0\r");
	At.SendCmd("AT+CIPRXGET=1\r");
	At.SendCmd("AT+CIPQRCLOSE=1\r");
	At.SendCmd("AT+CIPMODE=0\r");
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
bool GSM07::Test()
{
	// 避免在循环内部频繁构造和析构
	String cmd = "AT";
	for (int i = 0; i < 10; i++)
	{
		if (At.SendCmd(cmd, 500)) return true;

		Reset(false);
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
	//return At.Send("AT+GMR\r\n", "OK");
	//return At.Send("AT+GSN\r\n", "OK");
	return At.Send("ATI\r\n", "OK");
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
bool GSM07::Restore()
{
	return At.SendCmd("AT+RESTORE");
}

// 00:CMNET 10:CMHK 01:CHKT 11:HKCSL
void GSM07::SetAPN(cstring apn, bool issgp)
{
	String str;
	if (issgp)
		str = "AT+CIPCSGP=1";
	else
		str = "AT+CGDCONT=1,\"IP\"";
	str = str + ",\"" + apn + "\"\r";

	At.SendCmd(str);
}

String GSM07::GetIMSI() { return At.Send("AT+CIMI\r\n", "OK"); }
String GSM07::GetIMEI() { return At.Send("AT+EGMR\r\n", "OK"); }

/******************************** TCP/IP ********************************/
