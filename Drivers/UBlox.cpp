#include "UBlox.h"
#include "SerialPort.h"

UBlox::UBlox()
{
	Name	= "UBlox";
}

bool UBlox::OnOpen(bool isNew)
{
	auto sp	= (SerialPort*)Port;
	sp->SetBaudRate(Speed);
	if(isNew)
	{
		sp->Rx.SetCapacity(1024);
		sp->MaxSize		= 1024;
		sp->ByteTime	= 20;	// 拆包间隔
	}

	return true;
}

void UBlox::SetBaudRate(int baudRate)
{
	TS("UBlox::SetBaudRate");

	assert_param2(baudRate == 115200, "目前仅支持115200波特率的设定");
	
	// 构造波特率指令。默认115200
	byte cmd[] = {
		0xB5, 0x62, 0x06, 0x00, 0x14, 0x00,
		0x01, // PortID
		0x00, // reserved0
		0x00, 0x00, // txReady
		0xD0, 0x08, 0x00, 0x00, // mode。	0000 | 2reserved1 | 2charLen + 0 | parity | nStopBits
		0x00, 0xC2, 0x01, 0x00, // baudRate
		0x07, 0x00, // inProtoMask。	inUbx | inNmea | inRtcm
		0x03, 0x00, // outProtoMask。	outUbx | outNmea
		0x00, 0x00, // flags。0 | extendedTxTimeout
		0x00, 0x00, // reserved5
		0xC0, 0x7E	// CK_A CK_B
		};
	int len	= ArrayLength(cmd);
	int p	= 6 + 8;	// 头6字节，偏移8
	memcpy(cmd + p, &baudRate, 4);	// 小字节序
	// 修改校验。不含头尾各2字节
	byte CK_A = 0, CK_B = 0;
	for(int i=2; i<len-2-2; i++)
	{
		CK_A += cmd[i];
		CK_B += CK_A;
	}
	//cmd[len - 2]	= CK_A ;
	//cmd[len - 1]	= CK_B;

	// 发送命令
	Array bs(cmd, len);
	bs.Show(true);
	Port->Write(bs);
}

void UBlox::SetRate()
{
	// Navigation/Measurement Rate Settings
	byte cmd[]	= {
		0xB5, 0x62, 0x06, 0x08, 0x06, 0x00,
		0xE8, 0x03, // measRate,1000ms. Measurement Rate, GPS measurements are taken every measRate milliseconds
		0x01, 0x00, // navRate. Navigation Rate, in number of measurement cycles. This parameter cannot be changed, and must be set to 1.
		0x01, 0x00, // timeRef. Alignment to reference time: 0 = UTC time, 1 = GPS time
		0x01, 0x39 };
	//Port->Write(Array(cmd, ArrayLength(cmd)));
	Array bs(cmd, ArrayLength(cmd));
	bs.Show(true);
	Port->Write(bs);
}

void UBlox::EnterConfig()
{
	Open();

	Port->Register(NULL, NULL);
}

void UBlox::SaveConfig()
{
	// Clear, Save and Load configurations
	byte cmd[]	= {
		0xB5, 0x62, 0x06, 0x09, 0x0D, 0x00,
		0x00, 0x00, 0x00, 0x00, // clearMask. ioPort | msgConf | infMsg | navConf | rxmConf + rinvConf | antConf | logConf | ftsConf
		0xFF, 0xFF, 0x00, 0x00, // saveMask
		0x00, 0x00, 0x00, 0x00, // loadMask
		0x17, // deviceMask. devBBR | devFlash | devEEPROM | 0 | devSpiFlash
		0x31, 0xBF };
	//Port->Write(Array(cmd, ArrayLength(cmd)));
	Array bs(cmd, ArrayLength(cmd));
	bs.Show(true);
	Port->Write(bs);
}

/*bool UBlox::SetBaudRate(int baudRate)
{
	if(!Open()) return false;

	SendBaudRate(Port, baudRate);
	Sys.Sleep(1000);

	SendRate(Port);
	Sys.Sleep(1000);

	SaveConfig(Port);
	Sys.Sleep(1000);

	// 修改波特率，重新打开
	Close();

	auto sp	= (SerialPort*)Port;
	sp->SetBaudRate(baudRate);

	return Open();
}*/

void UBlox::OnReceive(const Array& bs, void* param)
{
	TS("UBlox::OnReceive");

	debug_printf("GPS[%d]=", bs.Length());
	String str(bs);
	str.Show(true);

	if(Buffer.Capacity() == 0) return;

	// 避免被截成两段
	if(bs[0] != '$')
	{
		if(Buffer.Length() == 0)
		{
			/*debug_printf("GPS数据断片[%d]=", bs.Length());
			String str(bs);
			str.Show(true);*/
		}

		//GPSDATA	+= bs;
	}
	else
	{
		Buffer.SetLength(0);
		Buffer.Copy(bs);
	}
}
