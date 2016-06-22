#include "TTime.h"
#include "SerialPort.h"

#include "Sim900A.h"

Sim900A::Sim900A()
{
	Port	= nullptr;

	Com		= COM_NONE;
	Speed	= 9600;
	Inited	= false;
	APN		= "CMNET";
	Led		= nullptr;
	Domain	= "pm25.peacemoon.cn";
}

Sim900A::~Sim900A()
{
	delete Port;
	Port = nullptr;
}

bool Sim900A::OnOpen()
{
	if(Port)
	{
#if DEBUG
		debug_printf("Sim900A::Open ");
		auto obj	= dynamic_cast<Object*>(Port);
		if(obj) obj->Show(true);
		//Port->Show(true);
#endif
	}
	else
	{
		debug_printf("Sim900A::Open COM%d %d \r\n", (byte)Com + 1, Speed);

		auto sp	= new SerialPort(Com, Speed);
		Port	= sp;

#if	!(defined(STM32F0) || defined(GD32F150))
		// 设置gprs发送大小
		sp->Tx.SetCapacity(512);
#endif
	}

	Inited	= false;

	return Port->Open();
}

void Sim900A::OnClose()
{
	// 先断开已有连接
	SendCmd("AT+CIPSHUT\r");

	Port->Close();
}

bool Sim900A::OnWrite(const Buffer& bs) { return Send(bs); }
uint Sim900A::OnRead(Buffer& bs) { return 0; }

String Sim900A::Send(cstring str, uint msTimeout)
{
	if(str)
	{
		String dat(str);
		Port->Write(dat);

#if DEBUG
		dat.Trim().Show(true);
#endif
	}

	String bs;
	bs.SetLength(bs.Capacity());

	TimeWheel tw(msTimeout);
	tw.Sleep	= 100;
	do
	{
		if(Port->Read(bs) >= 2) break;
	}while(!tw.Expired());

	if(bs.Length() > 4) bs.Trim();

	return bs;
}

bool Sim900A::SendCmd(cstring str, uint msTimeout, int times)
{
	for(int i=0; i<times; i++)
	{
		auto rt	= Send(str, msTimeout);
#if DEBUG
		rt.Show(true);
#endif

		if(!rt.StartsWith("ERROR"))  return true;

		// 设定小灯快闪时间，单位毫秒
		if(Led) Led->Write(50);

		// 如果进入了数据发送模式，则需要退出
		if(rt.Substring(2, 2) == "\r\n" || rt.Substring(1, 2) == "\r\n")
		{
			ByteArray end(0x1A, 1);
			Port->Write(end);
			Send("AT+CIPSHUT\r", msTimeout);
		}

		Sys.Sleep(350);
	}

	return false;
}

// 00:CMNET 10:CMHK 01:CHKT 11:HKCSL
void Sim900A::SendAPN(bool issgp)
{
	String str;
	//str.Clear();
	if(issgp)
		str = "AT+CIPCSGP=1";
	else
		str = "AT+CGDCONT=1,\"IP\"";
	str = str + ",\"" + APN + "\"\r";

	SendCmd(str.GetBuffer());
}

void Sim900A::SendDomain()
{
	String str;
	//str.Clear();
	str = "AT+CIPSTART=\"UDP\"";
	str = str + ",\"" + Domain + "\",\"3388\"\r";

	SendCmd(str.GetBuffer());
}

void Sim900A::Init(uint msTimeout)
{
	// ATE0 关闭回显
	// ATE1 开启回显
	SendCmd("ATE0\r");
	Send("AT+CIPSHUT\r");
	SendCmd("AT+CGCLASS=\"B\"\r");
	SendAPN(false);
	SendCmd("AT+CGATT=1\r");
	// 先断开已有连接
	//SendCmd("AT+CIPSHUT\r");
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
	//SendCmd("AT+CIPSTART=\"UDP\",\"pm25.peacemoon.cn\",\"3388\"\r", 3000, 3);
	SendDomain();

	// 读取CONNECT OK
	Inited = SendCmd(nullptr, 5000);
	//SendCmd("AT+CIPSHUT\r");
}

//以"AT"开头，以回车（<CR>）结尾			[AT+...\r]
//响应:<回车><换行><响应内容><回车><换行>	[\r\n....\r\n]
//使用最笨的任务方式进行进行处理
bool Sim900A::Send(const Buffer& bs)
{
	if(!Inited)
	{
		Close();
		//关闭串口以后500ms后重新配置
		Sys.Sleep(500);
		Init();
		return false;
	}

	// 进入发送模式
	auto b = SendCmd("AT+CIPSEND\r");
	
	//debug_printf("Sim900A::Send Data  Result:%d\r\n", b);
	
	if(!b)
	{		
		Init();
		return false;
	}

	Sys.Sleep(250);
	Port->Write(bs);

	Sys.Sleep(250);
	// 发送结束
	ByteArray end(0x1A, 1);
	Port->Write(end);
	
	// 把SEND OK读取回来
	if(SendCmd(nullptr))
	{
		//指示灯闪烁
		//Led.Start();
		// 设定小灯快闪时间，单位毫秒
		if(Led) Led->Write(500);
	}	

	return true;
}
