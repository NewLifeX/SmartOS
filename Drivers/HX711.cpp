#include "HX711.h"

#include "Time.h"

/*
XH711 有两个通道  A   B
A通道增益可以设置 64/128
B通道固定为32倍增益

HX711   有两个数字口   SCK   DOUT
DOUT：芯片从复位或断电状态进入工作状态后  DOUT 会保持高电平
此时高电平表示   芯片未准备好数据

SCK：
1.当 SCK 高电平60us 后，芯片进入端点状态，
	由芯片带动的电桥也将断电
2.芯片从复位或断电状态进入工作状态后
	A/D转换器需要四个数据输出周期才能稳定
	DOUT 才从高电平变为低电平
3.芯片从复位或断电状态进入工作状态后
	第一次数据默认使用  A通道 128倍增益
	第二次数据需要第一次读取数据时进行配置
	配置原则是 读完数据后 添加不同个数的SCK脉冲

当一切就绪  读数据时序：
SCK :-------________________________
DOUT:-------------------____________
	进入工作状态  	IC准备好数据

SCK上升沿驱动DOUT变动   即在SCK 下一次上升沿前读取DOUT便OK

24个SCK脉冲读取24位数据  高位在前
脉冲数必须是 25 26 27 之一  否则出错
25个脉冲   A通道128倍增益
26个脉冲   B通道32 倍增益
27个脉冲   A通道64 倍增益

读完数据后 DOUT 为高电平

当配置信息修改后  需要一段时间数据才能稳定
*/

HX711::HX711(Pin sck, Pin dout) : SCK(sck, true), DOUT(dout)
{
	assert(sck != P0, "SCK ERROR");
	assert(dout != P0, "DOUT ERROR");

	// 关闭 模块
	/*SCK.Open();
	SCK = true;*/

	Mode	= A128;
	Opened	= false;
}

/*HX711::~HX711()
{
	if(SCK)delete SCK;
	if(DOUT)delete DOUT;
}*/

bool HX711::Open()
{
	if(Opened) return true;

	/*if(!(SCK&&DOUT))
	{
		debug_printf("Pin ERROR\r\n");
		return false;
	}*/

	// 关闭 模块
	SCK.Open();
	SCK = true;

	//SCK->Open();
	SCK = false;
	DOUT.Open();

	Opened = true;

	return true;
}

bool HX711::Close()
{
	if(!Opened) return true;

	SCK	= true;
	DOUT.Close();

	Opened = false;

	return true;
}

uint HX711::Read()
{
	if(!Open())return 0;

	uint temp = 0x00000000;
	// 等待 IC 数据
	TimeWheel tw(0, 30);
	while(!tw.Expired() && DOUT);

	// 读取数据
	for(int i = 0;i < 24; i++)
	{
		SCK = true;
		temp <<= 1;
		if(DOUT)
			temp |= 0x01;
		//else
		//	temp &= 0xfffffffe;
		SCK = false;
	}
	// 设置模式部分
	for(int i = 0; i < Mode; i++)
	{
		SCK = true;
		//_nop();
		Sys.Delay(10);
		SCK = false;
	}
	return temp;
}
