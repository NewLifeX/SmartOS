#include "Kernel\TTime.h"
#include "Device\SerialPort.h"

#include "Sim900A.h"

Sim900A::Sim900A() :GSM07() {}

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
