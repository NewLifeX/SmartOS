#include "HMI.h"
#include "Device\SerialPort.h"
#include "Kernel\Sys.h"

#define NET_DEBUG DEBUG
//#define NET_DEBUG 0
#if NET_DEBUG
#define net_printf debug_printf
#else
#define net_printf(format, ...)
#endif

/******************************** HMI ********************************/
HMI::HMI()
{
	Port = nullptr;
}

HMI::~HMI()
{
	delete Port;
}

// 初始化HMI串口
void HMI::Init(COM idx, int baudrate)
{
	auto srp = new SerialPort(idx, baudrate);
	srp->Tx.SetCapacity(0x200);
	srp->Rx.SetCapacity(0x200);
	srp->MaxSize = 512;

	Init(srp);
}

// 注册HMI串口的数据到达
void HMI::Init(ITransport* port)
{
	Port = port;
	if (Port) Port->Register(OnPortReceive, this);
}

// 数据到达
uint HMI::OnPortReceive(ITransport* sender, Buffer& bs, void* param, void* param2)
{
	auto esp = (HMI*)param;
	return esp->OnReceive(bs, param2);
}

// 解析收到的数据
uint HMI::OnReceive(Buffer& bs, void* param)
{
	if (bs.Length() == 0) return 0;
	debug_printf("HMI::收到：");
	bs.AsString().Show(true);
	return 0;
}

// 向HMI发送数据指令
void HMI::Send(const String& cmd)
{
	Port->Write(cmd);
	SenDFlag();
}

// 向HMI发送指令结束符
void HMI::SenDFlag()
{
	byte by[3] = { 0xFF,0xFF,0xFF };
	Buffer bs(by, 3);
	Port->Write(bs);
}

