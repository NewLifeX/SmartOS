
#include "Blu40.h"

#include "Device\SerialPort.h"

// 思卡乐 CC2540

/*
默认情况
波特率：9600
停止位：1
奇偶校验：无

_cts 会在收到数据后拉低然后传送数据给mcu
_rts 拉低进行发送数据   一直保持低电平，模块会一直处于接收模式
*/

/*
"AT_BPS-"+X  波特率设置
X
0：1200
1：2400
2：4800
3：9600
4：19200
5：38400
6：57600
7：115200
设置两秒后生效
回复
"AT:BPS SET AFTER 2S \r\n\0"
"AT:ERR\r\n\0"
掉电记忆
*/
const byte AT_BPS[] = {0x41,0x54,0x3a,0x42,0x50,0x53,0x2d};//"AT_BPS-"

/*以下设置类回复状态 "AT:OK\r\n\0"  "AT:ERR\r\n\0"*/
/*
"AT:REN-"+Name 设置蓝牙名称  默认 RFScaler
掉电记忆
*/
//const byte AT_REN[] = "AT:REN-";
const byte AT_REN[] = {0x41,0x54,0x3a,0x52,0x45,0x4e,0x2d};

/*
"AT:PID-"+Data	自定义产品识别码 数据长度为2byte  默认为0x0000
掉电记忆
*/
//const byte AT_PID[] = "AT:PID-";
const byte AT_PID[] = {0x41,0x54,0x3a,0x50,0x49,0x44,0x2d};

/*
"AT:TPL-"+X  发送功率设置
X:
0:-23DB
1:-6DB
2:0DB
3:+4DB
*/
//const byte AT_TPL[] = "AT:TPL-";
const byte AT_TPL[] = {'A','T',':','T','P','L','-'};

//const byte ATOK[] = "AT:OK\r\n\0";
const byte ATOK[] = {'A','T',':','O','K','\r','\n','\0'};
//const byte ATERR[] = "AT:ERR\r\n\0";


const int TPLNum[] = {-23,-6,0,4};

Blu40::Blu40()
{
	Init();
}

Blu40::Blu40(Pin rts ,/*Pin cts,*/ Pin sleep, OutputPort* rst )
{
	Init();
	Init(rts,/*cts,*/sleep,rst);
}

void Blu40::Init()
{
	_rts = nullptr;
	//_cts = nullptr;
	_baudRate = 0;
	_sleep = nullptr;
}

void Blu40::Init(Pin rts,/*Pin cts,*/Pin sleep, OutputPort* rst)
{
	if(rts != P0)_rts = new OutputPort(rts); // 低电平有效
	if(sleep!=P0)_sleep = new OutputPort(sleep);
	if(_rts==nullptr)debug_printf("关键引脚_rts不可忽略");

	if(_sleep)*_sleep=false;
	*_rts = true;
	/*if(cts != P0)_cts = new InputPort(cts);
	_cts.Register();*/
	if(!rst)_rst = rst;
	Reset();
}

Blu40::~Blu40()
{
	if(_rts)delete _rts;
	//if(_cts)delete _cts;
	if(_rst)delete _rst;
}

void Blu40::Reset()
{
	// RESET ENABLE OF LOWER
	if(!_rst)return;
	*_rst = false;
	Sys.Delay(100);
	*_rst = true;
}

int const BPreserve[] = {1200,2400,4800,9600,19200,38400,57600,115200};  // 不是简单*=2能搞定的
bool Blu40::SetBP(int BP)
{
	if(BP>115200)
	{
		debug_printf("Blu不支持如此高的波特率\r\n");
	}

	byte bpnumIndex;
	for(bpnumIndex=0;BP!=BPreserve[bpnumIndex] && bpnumIndex<8;bpnumIndex++);
	if(BPreserve[bpnumIndex] != BP)return false;

	const Buffer bs((void*)AT_BPS, sizeof(AT_BPS));
	//byte buf[40];
	ByteArray ds;
	//byte BPSOK[] = "AT:BPS SET AFTER 2S \r\n\0"; // 坑人的需要ASIIC
	byte const BPSOK[] = {0x41,0x54,0x3A,0x42,0x50,0x53,0x20,0x45,0x54,0x20,0x41,0x46,0x54,0x45,0x52,0x20,0x32,0x53,0x0D,0x0A,0x00};
	auto sp	= dynamic_cast<SerialPort*>(Port);
	if(_baudRate != 0)
	{
		// 设置串口波特率
		sp->SetBaudRate(_baudRate);
		// 启用新波特率
		Port->Close();
		Port->Open();

		*_rts = false;
		Sys.Delay(150);

		Port->Write(bs);
		Port->Write(Buffer(&bpnumIndex, 1));	// 晕死，AT指令里面放非字符
		//Port->Write("\r\n",sizeof("\r\n"));	// 无需回车
		*_rts = true;

		Sys.Delay(500);
		Port->Read(ds);

		for(int j=0;j<(int)sizeof(BPSOK);j++)
		{
			if(ds[j] != BPSOK[j])
			{
				debug_printf("设置失败，重新调整波特率进行设置\r\n");
				break;
			}
			if(j==sizeof(BPSOK)-1)
			{
				debug_printf("设置成功，2S后启用新波特率\r\n");
				return true;
			}
		}
	}
	{
		int portBaudRateTemp = 115200;
		for(int i = 0;i<8;i++)
		{
			// 设置串口波特率
			sp->SetBaudRate(portBaudRateTemp);
			// 启用新波特率
			Port->Close();
			Port->Open();

			*_rts = false;
			Sys.Delay(170);
			Port->Write(bs);
			Port->Write(Buffer(&bpnumIndex, 1));
			//Port->Write("\r\n",sizeof("\r\n"));	// 无需回车
			*_rts = true;

			Sys.Delay(500);
			Port->Read(ds);
			//"AT:BPS SET AFTER 2S \r\n\0"
			//"AT:ERR\r\n\0"
			for(int j=0;j<(int)sizeof(BPSOK);j++)
			{
				if(ds[j] != BPSOK[j])
				{
					if(i==7)
					{
						debug_printf("设置失败，请检查现在使用的波特率是否正确\r\n");
						_baudRate = 0;
						return false;
					}
					debug_printf("设置失败，重新调整当前波特率进行设置\r\n");
					portBaudRateTemp=BPreserve[7-i-1];
					break;
				}
			}
		}
	_baudRate = portBaudRateTemp;
	}
	debug_printf("设置成功，2S后启用新波特率\r\n");
	return true;
}

bool Blu40::CheckSet()
{
	//byte buf[40];
	ByteArray bs;
	Port->Read(bs);
	for(int i=0;i<(int)sizeof(ATOK);i++)
	{
		if(bs[i] != ATOK[i])
		{
			debug_printf("设置失败\r\n");
			return false;
		}
	}
	debug_printf("设置成功\r\n");
	return true;
}

bool Blu40::SetName(cstring name)
{
	*_rts = false;
	Sys.Delay(170);
	Port->Write(Buffer((void*)AT_REN, sizeof(AT_REN)));
	Port->Write(String(name));
	bool ret = CheckSet();
	*_rts = true;
	return ret;
}

bool Blu40::SetPID(ushort pid)
{
	*_rts = false;
	Sys.Delay(170);
	Port->Write(Buffer((void*)AT_PID, sizeof(AT_PID)));
	Port->Write(Buffer(&pid, 2));
	bool ret = CheckSet();
	*_rts = true;
	return ret;
}

//const int TPLNum[] = {-23,-6,0,4};
bool Blu40::SetTPL(int TPLDB)
{
	byte TPLNumIndex ;
	for(TPLNumIndex= 0;TPLNumIndex<sizeof(TPLNum);TPLNumIndex++)
	{
		if(TPLNum[TPLNumIndex]==TPLDB)break;
	}
	*_rts = false;
	Sys.Delay(170);
	Port->Write(Buffer((void*)AT_TPL, sizeof(AT_TPL)));
	//byte temp = TPLNumIndex+'0';// 又是坑人的 非字符
	Port->Write(Buffer(&TPLNumIndex, 1));
	bool ret = CheckSet();
	*_rts = true;
	return ret;
}
