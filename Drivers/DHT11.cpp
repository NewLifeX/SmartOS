#include "DHT11.h"

DHT11::DHT11(Pin da)
{
	DA.Set(da);
}

DHT11::~DHT11()
{
	DA.Close();
}

void DHT11::Init()
{
	debug_printf("\r\nDHT11::Init DA=%s \r\n", DA.ToString().GetBuffer());

	DA.Open();
}

bool DHT11::Read(ushort& temp, ushort& humi)
{
	if(DA.Empty()) return false;

	/*输出模式*/
	/*主机拉低*/
	DA	= false;
	/*延时18ms*/
	Sys.Sleep(18);

	/*总线拉高 主机延时30us*/
	DA	= true;
	Sys.Delay(30);   //延时30us

	/*主机设为输入 判断从机响应信号*/

	/*判断从机是否有低电平响应信号 如不响应则跳出，响应则向下运行*/
	if(DA.ReadInput() != false) return false;

	/*轮询直到从机发出 的80us 低电平 响应信号结束*/
	while(!DA.ReadInput());

	/*轮询直到从机发出的 80us 高电平 标置信号结束*/
	while(DA.ReadInput());

	/*开始接收数据*/
	byte buf[5];
	for(int i=0; i<(int)ArrayLength(buf); i++)
		buf[i]	= ReadByte();

	/*读取结束，引脚改为输出模式*/
	/*主机拉高*/
	DA	= true;

	/*检查读取的数据是否正确*/
	if(buf[0] + buf[1] + buf[2] + buf[3] != buf[4]) return false;

	humi	=	buf[0] << 8;
	humi	|=	buf[1];

	temp	= 	buf[2] << 8;
	temp	|=	buf[3];

	return true;
}

byte DHT11::ReadByte()
{
	byte temp = 0;
	for(int i=0; i<8; i++)
	{
		/*每bit以50us低电平标置开始，轮询直到从机发出 的50us 低电平 结束*/
		while(!DA.ReadInput());

		/*DHT11 以27~28us的高电平表示“0”，以70us高电平表示“1”，
		通过检测60us后的电平即可区别这两个状态*/

		Sys.Delay(60); //延时60us

		if(DA.ReadInput())//60us后仍为高电平表示数据“1”
		{
			/*轮询直到从机发出的剩余的 30us 高电平结束*/
			while(DA.ReadInput());

			temp	|= (byte)(0x01 << (7-i));  // 把第7-i位置1
		}
	}
	return temp;
}

void DHT11::ChangePower(int level)
{
	// 进入低功耗时，直接关闭
	if(level) DA.Close();
}
