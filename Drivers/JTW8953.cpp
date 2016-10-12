#include "Sys.h"
#include "JTW8953.h"

#include "I2C.h"

#define ADDRESS_W		0xa6	// 从机的地址和写入标志
#define ADDRESS_R		0xa7	// 从机的地址和读取标志

#define ADDRESS_C		0xB1	// MCU配置起始地址
#define ADDRESS_T		0xC0	// 触摸键起始地址, 共有9个触摸键地址范围0xc0~0xD0

JTW8953::JTW8953()
{
	IIC		= nullptr;
	Port	= nullptr;
	// 7位地址
	Address	= 0x53;
}

JTW8953::~JTW8953()
{
	delete IIC;
	IIC	= nullptr;

	delete Port;
	Port	= nullptr;
}

void JTW8953::Init()
{
	debug_printf("\r\nJTW8953::Init Address=0x%02X \r\n", Address);

	IIC->Address	= Address << 1;
}

void JTW8953::SetSlide(bool slide)
{
	
}

bool JTW8953::SetConfig(const Buffer& bs) const
{
	return Write(ADDRESS_C, bs);
}

bool JTW8953::WriteKey(ushort index, byte data)
{
	if (!IIC) return false;
	// 只有10个按键
	if (index > 10)return false;


	IIC->Address = ADDRESS_W;
	index = index + ADDRESS_T;


	ByteArray buf(3);
	buf[0] = index;
	buf[1] = 0x10;
	buf[2] = data;


	return Write(0xff, buf);
}

byte JTW8953::Read(ushort addr)
{
	if (!IIC) return 0;
	// 只有10个按键
	if (addr > 10)return 0;

	IIC->Address = ADDRESS_R;
	addr = addr + ADDRESS_T;

	return IIC->Read(addr);
}

bool JTW8953::Write(uint addr, const Buffer& bs) const
{
	if (!IIC) return false;

	IIC->Address = ADDRESS_W;

	return IIC->Write(0, bs);
}

bool JTW8953::Read(uint addr, Buffer& bs) const
{
	if (!IIC) return false;

	IIC->Address = ADDRESS_R;

	uint len = IIC->Read(0, bs);
	if (len == 0)return false;
	if (len != bs.Length()) bs.SetLength(len);

	return true;
}

void JTW8953::JTW8953Test()
{
	SoftI2C iic;
	iic.SetPin(PB15, PA12);

	InputPort port(PD13);
	
	JTW8953 jtw;
	jtw.IIC = &iic;

	// 读取数据缓存
	ByteArray buf(18);
	// 设置为滑条模式
	ByteArray buf2(4);
	buf2[0] = 0xB1;
	buf2[1] = 0x23;
	buf2[2] = 0x33;
	buf2[3] = 0x03;

	// 配置
	jtw.Write(0xff, buf2);
	Sys.Sleep(100);

	while (true)
	{
		// 分别写入和读取
		for (size_t i = 0; i < 10; i++)
		{
			//Sys.Sleep(1000);
			//debug_printf("第%d键位写入数据%d\r\n", i, i % 2);
			//jtw.WriteKey(i, i % 2);
			Sys.Sleep(1000);
			jtw.Read(0xff, buf);
			debug_printf("读取键位数据\r\n");
			buf.Show(true);
		}
	}
}
