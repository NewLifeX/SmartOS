#include "Kernel\Sys.h"
#include "JTW8953.h"

#include "Device\I2C.h"

#define ADDRESS			0xa6	// 从机的地址和写入标志

#define ADDRESS_C		0xB1	// MCU配置起始地址
#define ADDRESS_T		0xC0	// 触摸键起始地址, 共有9个触摸键地址范围0xc0~0xD0

JTW8953::JTW8953()
{
	IIC = nullptr;
	Port = nullptr;
	// 7位地址
	Address = ADDRESS;
}

JTW8953::~JTW8953()
{
	delete IIC;
	IIC = nullptr;

	delete Port;
	Port = nullptr;
}

void JTW8953::Init()
{
	debug_printf("\r\nJTW8953::Init Address=0x%02X \r\n", Address);

	IIC->Address = Address;
}

void JTW8953::SetSlide(bool slide)
{

}

bool JTW8953::SetConfig(const Buffer& bs) const
{
	return Write(bs);
}

bool JTW8953::WriteKey(ushort index, byte data)
{
	if (!IIC) return false;
	// 只有10个按键
	if (index > 10)return false;
	index = index + ADDRESS_T;

	ByteArray buf(3);
	buf[0] = index;
	buf[1] = 0x10;
	buf[2] = data;

	return Write(buf);
}


bool JTW8953::Write(const Buffer& bs) const
{
	if (!IIC) return false;
	return IIC->Write(0, bs);
}

bool JTW8953::Read(Buffer& bs) const
{
	if (!IIC) return false;

	int len = IIC->Read(0, bs);
	if (len == 0)return false;
	if (len != bs.Length()) bs.SetLength(len);

	return true;
}


static  JTW8953 JTW;

static void ReadJTW(InputPort& port, bool down)
{
	ByteArray buf(18);
	// 分别写入和读取
	Sys.Sleep(1000);
	JTW.Read(buf);
	debug_printf("读取键位数据\r\n");
	buf.Show(true);

}
void JTW8953::JTW8953Test()
{
	SoftI2C iic;
	iic.SetPin(PB15, PA12);

	InputPort INT(PB13);

	INT.Press.Bind(ReadJTW);
	JTW.IIC = &iic;

	JTW.Init();

	// 设置为滑条模式
	ByteArray buf2(4);
	buf2[0] = 0xB1;
	buf2[1] = 0x23;
	buf2[2] = 0x33;
	buf2[3] = 0x03;

	// 配置
	JTW.Write(buf2);
	Sys.Sleep(100);

}
