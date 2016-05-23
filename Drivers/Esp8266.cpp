
#include "ESP8266.h"

void EspTest(void * param)
{
	AiESP esp(COM4);
	//esp.Port.SetBaudRate(115200);
	esp.Port.Open();

	Sys.Sleep(50);				//

	if (esp.GetMode() != Station)	// Station模式
		esp.SetMode(Station);

	String ate = "ATE1";			// 开回显
	esp.SendCmd(ate);

	String ssid = "yws007";
	String pwd = "yws52718";

	//String ssid = "FAST_2.4G";
	//String pwd = "yws52718*";
	bool isjoin = esp.JoinAP(ssid, pwd);
	if (isjoin)debug_printf("\r\nJoin ok\r\n");
	else debug_printf("\r\nJoin not ok\r\n");
	Sys.Sleep(1000);
	esp.UnJoinAP();
	Sys.Sleep(1000);
}

String busy = "busy p...";
String discon = "WIFI DISCONNECT";
String conn = "WIFI CONNECTED";
String okstr = "OK";
String gotIp = "WIFI GOT IP";


AiESP::AiESP()
{
	Mode = Unknown;
}

AiESP::AiESP(COM com, AiEspMode mode)
{
	Port.Set(com,115200);
	Port.ByteTime = 1;
	Port.MinSize = 1;
	Mode = mode;
}
/*
发送：
"AT+CWMODE=1
"
返回：
"AT+CWMODE=1

OK
"
*/
bool AiESP::SetMode(AiEspMode mode)
{
	String str = "AT+CWMODE=";
	String cmd;
	switch (mode)
	{
	case Unknown:
		return false;
		//break;
	case Station:
		cmd = str + '1';
		break;
	case Ap:
		cmd = str + '2';
		break;
	case Both:
		cmd = str + '3';
		break;
	default:
		return false;

	}
	if (SendCmd(cmd))
	{
		Mode = mode;
		return true;
	}
	else
		return false;
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
AiEspMode AiESP::GetMode()
{
	debug_printf("GetMode\r\n");
	String str = "AT+CWMODE?\r\n";
	Send(str);
	MemoryStream rsms;
	if (RevData(rsms, 1000) == 0)return Unknown;

	String mo1 = "+CWMODE:1";
	String mo2 = "+CWMODE:2";
	String mo3 = "+CWMODE:3";

	String rsstr((const char*)rsms.GetBuffer(), rsms.Position());
	Mode = Unknown;
	if (rsstr.IndexOf(mo1) != -1)Mode = Station;
	if (rsstr.IndexOf(mo2) != -1)Mode = Ap;
	if (rsstr.IndexOf(mo3) != -1)Mode = Both;

	debug_printf("Mode = %d\r\n", Mode);

	return Mode;
}
// 发送数据  并按照参数条件 适当返回收到的数据
void  AiESP::Send(Buffer & dat)
{
	if (dat.Length() != 0)
	{
		Port.Write(dat);
		// 仅仅 send 不需要刷出去
		//if (needRead)
		//	Port.Flush();
	}
}

int AiESP::RevData(MemoryStream &ms, uint timeMs)
{
	byte temp[64];
	Buffer bf(temp, sizeof(temp));
	TimeWheel tw(0);
	tw.Reset(timeMs / 1000, timeMs % 1000);
	while (!tw.Expired())
	{
		// Port.Read 会修改 bf 的 length 为实际读到的数据长度 
		// 所以每次都需要修改传入 buffer 的长度为有效值
		Sys.Sleep(10);
		bf.SetLength(sizeof(temp));
		Port.Read(bf);
		if (bf.Length() != 0)ms.Write(bf);// 把读到的数据
	}
	return	ms.Position();
}
// str 不需要换行
bool AiESP::SendCmd(String &str, uint timeOutMs)
{
	String cmd(str);
	cmd += "\r\n";
	Send(cmd);
	MemoryStream rsms;
	auto rslen = RevData(rsms, timeOutMs);
	debug_printf("\r\n ");
	str.Show(false);
	debug_printf(" SendCmd rev len:%d\r\n", rslen);
	if (rslen == 0)return false;

	String rsstr((const char *)rsms.GetBuffer(), rsms.Position());
	//rsstr.Show(true);

	bool isOK = true;
	int index = 0;
	int indexnow = 0;

	index = rsstr.IndexOf(str);
	if (index == -1)
	{
		// 没有找到命令本身就表示不通过
		isOK = false;
		debug_printf("not find cmd\r\n");
	}
	else
	{
		indexnow = index + str.Length();
		// 去掉busy
		index = rsstr.IndexOf(busy, indexnow);
		if (index != -1 && index - 5 < indexnow)
		{
			// 发现有 busy 并在允许范围就跳转    没有就不动作
			// debug_printf("find busy\r\n");
			indexnow = index + busy.Length();
		}

		index = rsstr.IndexOf(okstr, indexnow);
		if (index == -1 || index - 5 > indexnow)
		{
			// 没有发现  或者 不在范围内就认为是错的
			debug_printf("not find ok\r\n");
			isOK = false;
		}
		else
		{
			// 到达这里表示 得到正确的结果了
			indexnow = index + okstr.Length();
		}
	}
	// 还有多余数据 就扔出去
	//if(rslen -indexnow>5)xx()
	if (isOK)
		debug_printf(" Cmd OK\r\n");
	else
		debug_printf(" Cmd Fail\r\n");
	return isOK;
}
/*
发送：
"AT+CWJAP="yws007","yws52718"
"
返回：	( 一般3秒钟能搞定   密码后面位置会停顿，  WIFI GOT IP 前面也会停顿 )
"AT+CWJAP="yws007","yws52718"WIFI CONNECTED
WIFI GOT IP

OK
"
也可能  (已连接其他WIFI)  70bytes
"AT+CWJAP="yws007","yws52718" WIFI DISCONNECT
WIFI CONNECTED
WIFI GOT IP

OK
"
密码错误返回
"AT+CWJAP="yws007","7" +CWJAP:1

FAIL
"
*/
bool AiESP::JoinAP(String & ssid, String &pwd)
{
	String str = "AT+CWJAP=";
	str.Show(true);
	str = str + "\"" + ssid + "\",\"" + pwd + "\"";

	String cmd = str + "\r\n";

	// 因为时间跨度很长  所以自己来捞数据
	Send(cmd);
	MemoryStream rsms;
	auto rslen = RevData(rsms, 10000);	// 这里注定捞不干净  哪怕延时20s都不行  原因不明
	debug_printf("\r\nRevData len:%d\r\n", rslen);
	String rsstr((const char *)rsms.GetBuffer(), rsms.Position());
	//rsstr.Show(true);
	int index = 0;
	int indexnow = 0;

	if (indexnow + 20 > rslen)			// 补捞  重组字符串   因为不动  rsms.Position() 所以数据保留不变
	{
		rslen = RevData(rsms, 1000);
		debug_printf("\r\nRevData len:%d\r\n", rslen);
		rsstr = String((const char *)rsms.GetBuffer(), rsms.Position());
	}

	index = rsstr.IndexOf("AT+CWJAP");
	if (index != -1)
		indexnow = index;
	else
	{
		debug_printf("not find cmd\r\n");
		return false;
	}

	// 补捞  重组字符串   因为不动rsms.Position()  所以数据保留不变
	if (cmd.Length()+discon.Length() > rslen)
	{
		rslen = RevData(rsms, 1000);
		debug_printf("\r\nRevData len:%d\r\n", rslen);
		rsstr = String((const char *)rsms.GetBuffer(), rsms.Position());
	}
	// 干掉 WIFI DISCONNECT
	index = rsstr.IndexOf(discon, indexnow);
	if (index != -1)indexnow = index + discon.Length();

	// 补捞  重组字符串   因为不动  rsms.Position() 所以数据保留不变
	auto comNum = index <= 0 ? cmd.Length() + conn.Length() : indexnow + conn.Length();
	if (comNum > rslen)
	{
		rslen = RevData(rsms, 1000);
		debug_printf("\r\nRevData len:%d\r\n", rslen);
		rsstr = String((const char *)rsms.GetBuffer(), rsms.Position());
	}

	index = rsstr.IndexOf(conn, indexnow);
	if (index == -1)
	{
		debug_printf("\r\nindex:%d,  find conn not ok\r\n",index);
		return false;
	}
	else
	{
		// 到达这里表示  已经连接到了
		indexnow = index + conn.Length();
	}

	// 补捞  重组字符串   因为不动  rsms.Position() 所以数据保留不变
	if (indexnow + 20 > rslen)
	{
		rslen = RevData(rsms, 1000);
		debug_printf("\r\nRevData len:%d\r\n", rslen);
		rsstr = String((const char *)rsms.GetBuffer(), rsms.Position());
	}

	index = rsstr.IndexOf(gotIp, indexnow);
	if (index == -1 || index - 5 > indexnow)
	{
		debug_printf("not find gotIp\r\n");
		return false;
	}
	else
	{
		// 到达这里表示  已经连接到了
		indexnow = index + gotIp.Length();
	}

	index = rsstr.IndexOf(okstr, indexnow);
	if (index == -1 || index - 5 > indexnow)
	{
		// 没有发现  或者 不在范围内就认为是错的
		debug_printf("not find ok\r\n");
		return false;
	}
	else
	{
		// 到达这里表示 得到正确的结果了
		indexnow = index + okstr.Length();
	}

	debug_printf("\r\nJoinAP  ok\r\n");
	return true;
}
/*
返回：
"AT+CWQAP WIFI DISCONNECT

OK
"
*/
bool AiESP::UnJoinAP()
{
	String str = "AT+CWQAP";

	String cmd(str);
	cmd += "\r\n";
	Send(cmd);
	MemoryStream rsms;
	auto rslen = RevData(rsms, 2000);
	debug_printf("\r\n ");
	str.Show(false);

	debug_printf(" SendCmd rev len:%d\r\n", rslen);
	if (rslen == 0)return false;
	String rsstr((const char *)rsms.GetBuffer(), rsms.Position());
	//rsstr.Show(true);

	bool isOK = true;
	int index = 0;
	int indexnow = 0;

	index = rsstr.IndexOf(str);
	if (index == -1)
	{
		// 没有找到命令本身就表示不通过
		isOK = false;
		debug_printf("not find cmd\r\n");
	}
	else
	{
		indexnow = index + str.Length();
		// 去掉busy
		index = rsstr.IndexOf(busy, indexnow);
		if (index != -1 && index - 5 < indexnow)
		{
			// 发现有 busy 并在允许范围就跳转    没有就不动作
			// debug_printf("find busy\r\n");
			indexnow = index + busy.Length();
		}
		// 断开连接
		index = rsstr.IndexOf(discon, indexnow);
		if (index != -1 && index - 5 < indexnow)
		{
			// 断开连接
			indexnow = index + discon.Length();
		}
		// 去掉busy
		index = rsstr.IndexOf(busy, indexnow);
		if (index != -1 && index - 5 < indexnow)
		{
			indexnow = index + busy.Length();
		}

		index = rsstr.IndexOf(okstr, indexnow);
		if (index == -1 || index - 5 > indexnow)
		{
			// 没有发现  或者 不在范围内就认为是错的
			debug_printf("not find ok\r\n");
			isOK = false;
		}
		else
		{
			// 到达这里表示 得到正确的结果了
			indexnow = index + okstr.Length();
		}
	}
	// 还有多余数据 就扔出去
	//if(rslen -indexnow>5)xx()
	if (isOK)
		debug_printf(" Cmd OK\r\n");
	else
		debug_printf(" Cmd Fail\r\n");
	return isOK;


}
/*
开机自动连接WIFI
*/
bool AiESP::AutoConn(bool enable)
{
	String cmd = "AT+ CWAUTOCONN=";
	if (enable)cmd += '1';
	else cmd += '0';
	return SendCmd(cmd);
}

