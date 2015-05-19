
#include "Blu40.h"
// 司卡乐 CC2540

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
const byte AT_BPS[] = "AT:BPS-";

/*以下设置类回复状态 "AT:OK\r\n\0"  "AT:ERR\r\n\0"*/
/*
"AT:REN-"+Name 设置蓝牙名称  默认 RFScaler
掉电记忆
*/
const byte AT_REN[] = "AT::REN-";

/*
"AT:PID-"+Data	自定义产品识别码 数据长度为2byte  默认为0x0000
掉电记忆
*/
const byte AT_PID[] = "AT:PID-";

/*
"AT:TPL-"+X  发送功率设置
X:
0:-23DB
1:-6DB
2:0DB
3:+4DB
*/
const byte AT_TPL[] = "AT:TPL-";

const byte ATOK[] = "AT:OK\r\n\0";
const byte ATERR[] = "AT:ERR\r\n\0";

Blu40::Blu40()
{
	_rts = NULL;
	_cts = NULL;
}

Blu40::Blu40(ITransport *port,Pin rts = P0 ,Pin cts = P0, OutputPort * rst = NULL)
{
	assert_ptr(port);
	Init(port,rts,cts,rst);
}

void Blu40::Init(ITransport *port = NULL,Pin rts = P0,Pin cts = P0,OutputPort * rst = NULL)
{
	if(!port) _port = port;
	if(rts != P0)_rts = new OutputPort(rts);
	//if(cts != P0)_cts = new InputPort(cts);
	//_cts.Register();
	if(!rst)_rst = rst;
	Reset();
}

void Blu40::Reset()
{
	// RESET ENABLE OF LOWER
	if(!_rst)return;
	*_rst = false;
	for(int i = 0;i<100;i++);
	*_rst = true;
}

bool Blu40::SetBP(uint BP)
{
	if(BP>115200)
	{
		debug_prinf("Blu不支持如此高的波特率");
	}
	//const byte AT_BPSNum[] = {'0','1','2','3','4','5','6','7'};
	byte bpnumIndex;
	for( bpnumIndex =0,_bp=1200;_bp == BP;_bp<<=1,bpnumIndex++ );
	
	_port->Write(AT_BPS,sizof(AT_BPS));
	//_port->Write(AT_BPSNum[bpnumIndex],1);
	_port->Write(bpnumIndex+'0',1);
	
	byte buf[40];
	_port->Read(buf,40);
	//"AT:BPS SET AFTER 2S \r\n\0"
	//"AT:ERR\r\n\0"
	const byte BPSOK[] = "AT:BPS SET AFTER 2S \r\n\0";
	for(int i=0,void *p = buf,void *p2 = BPSOK;i<sizeof(BPSOK);i++)
	{
		if(*p != *p2)
		{
			debug_prinf("设置失败，请检查现在使用的波特率是否正确");
			return false;
		}
	}
	debug_prinf("设置成功，2S后启用新波特率\r\n");
	return true;
}

bool Blu40::CheckSet()
{
	byte buf[40];
	_port->Read(buf,40);
	for(int i=0,void *p = buf,void *p2 = ATOK;i<sizeof(ATOK);i++)
	{
		if(*p != *p2)
		{
			debug_prinf("设置失败");
			return false;
		}
	}
	debug_prinf("设置成功\r\n");
	return true;
}

bool Blu40::SetName(string name)
{
	_port->Write(AT_REN,sizof(AT_REN));
	_port->Write(name,sizeof(name));
	return CheckSet();
}

bool Blu40::SetPID(ushort pid)
{
	_port->Write(AT_PID,sizof(AT_REN));
	_port->Write(&pid,2);
	return CheckSet();
}

//const int TPLNum[] = {-23,-6,0,4};
bool Blu40::SetTPL(int TPLDB)
{
	byte TPLNumIndex ;
	for(TPLNumIndex= 0;TPLNumIndex<sizof(TPLNum);TPLNumIndex++)
	{
		if(TPLNum[TPLNumIndex]==TPLDB)break;
	}
	_port->Write(AT_TPL,sizof(AT_TPL));
	_port->Write(TPLNumIndex+'0',1);
	return CheckSet();
}
