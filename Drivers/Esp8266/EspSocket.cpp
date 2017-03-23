#include "EspSocket.h"

#define NET_DEBUG DEBUG
//#define NET_DEBUG 0
#if NET_DEBUG
#define net_printf debug_printf
#else
#define net_printf(format, ...)
#endif

/******************************** Socket ********************************/

EspSocket::EspSocket(Esp8266& host, NetType protocol, byte idx)
	: _Host(host)
{
	_Index = idx;
	_Error = 0;

	Host = &host;
	Protocol = protocol;
}

EspSocket::~EspSocket()
{
	Close();
}

bool EspSocket::OnOpen()
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

	_Host.SetMux(true);

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

	String cmd = "AT+CIPSTART=";
	cmd = cmd + _Index + ",";

	if (Protocol == NetType::Udp)
		cmd += "\"UDP\"";
	else if (Protocol == NetType::Tcp)
		cmd += "\"TCP\"";

	auto rm = Server;
	if (!rm) rm = Remote.Address.ToString();

	// 设置端口目的(远程)IP地址和端口号
	cmd = cmd + ",\"" + rm + "\"," + Remote.Port;
	// 设置自己的端口号
	if (Local.Port) cmd = cmd + ',' + Local.Port;
	// UDP传输属性。0，收到数据不改变远端目标；1，收到数据改变一次远端目标；2，收到数据改变远端目标

	/*if(Remote.Address == IPAddress::Broadcast())
		cmd += ",2";
	else*/
	if (Protocol == NetType::Udp) cmd += ",0";

	// 打开Socket。有OK/ERROR/ALREADY CONNECTED三种
	auto rt = _Host.At.Send(cmd + "\r\n", "OK", "ERROR", 10000, false);
	if (!rt.Contains("OK") && !rt.Contains("ALREADY CONNECTED"))
	{
		net_printf("协议 %d, %d 打开失败 \r\n", (byte)Protocol, Remote.Port);
		return false;
	}

	// 清空一次缓冲区
	/*cmd	= "AT+CIPBUFRESET=";
	_Host.SendCmd(cmd + _Index);*/
	_Error = 0;

	return true;
}

void EspSocket::OnClose()
{
	String cmd = "AT+CIPCLOSE=";
	cmd += _Index;

	_Host.At.SendCmd(cmd, 1600);
}

// 接收数据
uint EspSocket::Receive(Buffer& bs)
{
	if (!Open()) return 0;

	return 0;
}

// 发送数据
bool EspSocket::Send(const Buffer& bs)
{
	if (!Open()) return false;

	String cmd = "AT+CIPSEND=";
	cmd = cmd + _Index + ',' + bs.Length() + "\r\n";

	return SendData(cmd, bs);
}

bool EspSocket::SendData(const String& cmd, const Buffer& bs)
{
	// 重发3次AT指令，避免busy
	int i = 0;
	for (i = 0; i < 3; i++)
	{
		//auto rt	= _Host.Send(cmd, ">", "OK", 1600);
		// 不能等待OK，而应该等待>，因为发送期间可能给别的指令碰撞
		auto rt = _Host.At.Send(cmd, ">", "ERROR", 1600, false);
		if (rt.Contains(">")) break;

		Sys.Sleep(500);
	}
	if (i < 3 && _Host.At.Send(bs.AsString(), "SEND OK", "ERROR", 1600, false).Contains("SEND OK"))
	{
		return true;
	}

	// 发送失败，关闭链接，下一次重新打开
	if (++_Error >= 10)
	{
		_Error = 0;

		Close();
	}

	return false;
}

bool EspSocket::OnWrite(const Buffer& bs) { return Send(bs); }
uint EspSocket::OnRead(Buffer& bs) { return Receive(bs); }

// 收到数据
void EspSocket::OnProcess(const Buffer& bs, const IPEndPoint& remote)
{
	OnReceive((Buffer&)bs, (void*)&remote);
}
