#include "Sim900A.h"
#include "SerialPort.h"
#include "Time.h"

Sim900A::Sim900A()
{
	Port	= NULL;

	Com		= COM_NONE;
	Speed	= 9600;
	APN		= NULL;
	Led		= NULL;
}

Sim900A::~Sim900A()
{
	delete Port;
	Port = NULL;
}

bool Sim900A::OnOpen()
{
	if(Port)
	{
		debug_printf("\r\n Sim900A::Open %s \r\n", Port->ToString());
	}
	else
	{
		debug_printf("\r\n Sim900A::Open COM%d %d \r\n", (byte)Com + 1, Speed);

		auto sp	= new SerialPort(Com, Speed);
		Port	= sp;

		// 设置gprs发送大小
		sp->Tx.SetCapacity(512);
	}

	return Port->Open();
}

void Sim900A::OnClose()
{
	// 先断开已有连接
	SendCmd("AT+CIPSHUT\r", 5);

	Port->Close();
}

bool Sim900A::OnWrite(const Array& bs) { return Send(bs); }
uint Sim900A::OnRead(Array& bs) { return 0; }

String Sim900A::Send(const char* str, uint sTimeout)
{
	if(str)
	{
		String dat(str, 0);
		Port->Write(dat);

#if DEBUG
		dat.Trim().Show(true);
#endif
	}

	String bs;
	bs.SetLength(bs.Capacity());

	TimeWheel tw(sTimeout);
	tw.Sleep	= 1;
	while(!tw.Expired())
	{
		if(Port->Read(bs) >= 2) break;
	}

	if(bs.Length() > 4) return bs.Trim();

	return bs;
}

bool Sim900A::SendCmd(const char* str, uint sTimeout)
{
	for(int i=0; i<sTimeout; i++)
	{
		auto rt	= Send(str, sTimeout);
#if DEBUG
		rt.Show(true);
#endif

		if(rt.Sub(0, 5) != "ERROR")  return true;

		// 设定小灯快闪时间，单位毫秒
		if(Led) Led->Write(500);

		// 如果进入了数据发送模式，则需要退出
		if(rt.Sub(2, 2) == "\r\n" || rt.Sub(1, 2) == "\r\n")
		{
			ByteArray end(0x1A, 1);
			Port->Write(end);
			Send("AT+CIPSHUT\r", sTimeout);
		}

		Sys.Sleep(1000);
	}

	return false;
}

// 00:CMNET 10:CMHK 01:CHKT 11:HKCSL
void Sim900A::SendAPN(bool issgp)
{
	String str;
	str.Clear();
	if(issgp)
		str = "AT+CIPCSGP=1";
	else
		str = "AT+CGDCONT=1,\"IP\"";
	str = str + ",\"" + APN + "\"\r";

	SendCmd(str.GetBuffer());
}

void Sim900A::Init(uint sTimeout)
{
	// ATE0 关闭回显
	// ATE1 开启回显
	SendCmd("ATE0\r");
	SendCmd("AT+CGCLASS=\"B\"\r");
	SendAPN(false);
	SendCmd("AT+CGATT=1\r");
	// 先断开已有连接
	SendCmd("AT+CIPSHUT\r", 5);
	//设置APN
	SendAPN(true);
	SendCmd("AT+CLPORT=\"UDP\",\"3399\"\r");
	// IP设置方式
	//SendCmd("AT+CIPSTART=\"UDP\",\"183.63.213.113\",\"3388\"\r");
	// 域名设置方式
	SendCmd("AT+CIPMUX=0\r");
	SendCmd("AT+CIPRXGET=1\r");
	SendCmd("AT+CIPQRCLOSE=1\r");
	SendCmd("AT+CIPMODE=0\r");
	SendCmd("AT+CIPSTART=\"UDP\",\"pm25.peacemoon.cn\",\"3388\"\r");

	// 读取CONNECT OK
	Opened = SendCmd(NULL, sTimeout);
	//SendCmd("AT+CIPSHUT\r");
}

//以"AT"开头，以回车（<CR>）结尾			[AT+...\r]
//响应:<回车><换行><响应内容><回车><换行>	[\r\n....\r\n]
//使用最笨的任务方式进行进行处理
bool Sim900A::Send(const Array& bs)
{
	if(!Opened) Init();
	if(!Opened) return false;

	// 进入发送模式
	if(!SendCmd("AT+CIPSEND\r", 5))
	{
		Close();
		return false;
	}

	Port->Write(bs);

	// 发送结束
	ByteArray end(0x1A, 1);
	Port->Write(end);

	//指示灯闪烁
	//Led.Start();
	// 设定小灯快闪时间，单位毫秒
	if(Led) Led->Write(500);

	// 把SEND OK读取回来
	SendCmd(NULL);
	//SendCmd("AT+CIPSHUT\r");

	return true;
}
