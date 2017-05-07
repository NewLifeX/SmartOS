#include "Kernel\TTime.h"
#include "Device\SerialPort.h"

#include "Sim900A.h"

Sim900A::Sim900A() :GSM07() {
	At.DataKey = "\r\n+IPD,";
}

bool Sim900A::OnOpen()
{
	// 退出传输
	String str((byte)0x1A, 16);
	At.Send(str);

	return GSM07::OnOpen();
}

// 数据到达
void Sim900A::OnReceive(Buffer& bs)
{
	// \r\n+IPD,61:xxx
	auto str = bs.AsString();
	int p = str.IndexOf(":");
	if (p <= 0) return;

	int len = str.Substring(0, p).ToInt();
	// 检查长度
	if (p + 1 + len > bs.Length()) len = bs.Length() - p - 1;
	auto data = bs.Sub(p + 1, len);

	OnProcess(0, data, _Remote);
}

bool Sim900A::IPSend(int index, const Buffer& data)
{
	assert(data.Length() <= 1024, "每次最多发送1024字节");

	/*
	AT+CIPSEND=5,”12345” //同步发送字符串
	AT+CIPSEND=5 //出现”>”后可以发送5个字节的二进制数据
	AT+CIPSEND //出现”>”后可以发送以CTRL+Z结尾的字符串
	*/

	/*String cmd = "AT+CIPSEND=";
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
	return SendData(cmd, data);*/

	// 退出传输
	String str((byte)0x1A, 16);
	At.Send(str);

	String cmd = "AT+CIPSEND\r\n";

	int i = 0;
	for (i = 0; i < 3; i++)
	{
		//auto rt	= _Host.Send(cmd, ">", "OK", 1600);
		// 不能等待OK，而应该等待>，因为发送期间可能给别的指令碰撞
		auto rt = At.Send(cmd, ">", "ERROR", 1600, false);
		if (rt.Contains(">")) break;

		Sys.Sleep(500);
	}
	if (i >= 3) return false;

	At.Send(data.AsString(), 1000, false);

	//String str((byte)0x1A, 16);
	return At.Send(str, "SEND OK", "ERROR", 1600).Contains("SEND OK");
}

/*void Sim900A::Init(uint msTimeout)
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
}*/
