
#include "Blu40.h"

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
*/
const byte AT_BPS[] = "AT:BPS-";

/*以下设置类回复状态 "AT:OK\r\n\0"  "AT:ERR\r\n\0"*/
/*
"AT:REN-"+Name 设置蓝牙名称
*/
const byte AT_REN[] = "AT::REN-";
/*
"AT:PID-"+Data	自定义产品识别码 数据长度为2byte  默认为0x0000
*/




Blu40::Blu40()
{
	_rts = NULL;
	_cts = NULL;
}


Blu40:Blu40(ITransport *port,Pin rts = P0 ,Pin cts = P0, OutputPort * rst = NULL)
{
	assert_ptr(port);
	Init(port,rts,cts,rst);
}

void Blu40:Init(ITransport *port = NULL,Pin rts = P0,Pin cts = P0,OutputPort * rst = NULL)
{
	if(!port) _port = port;
	if(rts != P0)_rts = new OutputPort(rts);
	//if(cts != P0)_cts = new InputPort(cts);
	//_cts.Register();
	if(!rst)_rst = rst;
	Reset();
}

void Blu40:Reset()
{
	// RESET ENABLE OF LOWER
	if(!_rst)return;
	*_rst = false;
	for(int i = 0;i<100;i++);
	*_rst = true;
}





