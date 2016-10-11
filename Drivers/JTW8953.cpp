#include "JTW8953.h"

#define ADDRESS_W		0xa6	//从机的地址和写入标志
#define ADDRESS_R		0xa7	//从机的地址和读取标志

#define ADDRESS_C		0xB1	// MCU配置起始地址
#define ADDRESS_T		0xC0	// 触摸键起始地址, 共有9个触摸键地址范围0xc0~0xD0

JTW8953::JTW8953()
{
	IIC		= nullptr;
	// 7位地址，到了I2C那里，需要左移1位
	Address	= 0xa6;
}

JTW8953::~JTW8953()
{
	delete IIC;
	IIC = nullptr;
}

void JTW8953::Init()
{
	debug_printf("\r\nJTW8953::Init Address=0x%02X \r\n", Address);

	IIC->SubWidth	= 1;
	IIC->Address	= Address;
}

bool JTW8953::SetConfig(const Buffer& bs) const
{
   return Write(ADDRESS_C, bs);
}

bool JTW8953::WriteKey(ushort addr, byte data)
{
	if(!IIC) return false;
	// 只有10个按键
	if (addr > 10)return false;

	IIC->Address = ADDRESS_W;
	addr = addr + ADDRESS_T;
	
	return IIC->Write(addr & 0xFF, data);
}

byte JTW8953::Read(ushort addr)
{
	if(!IIC) return 0;
	// 只有10个按键
	if (addr > 10)return 0;

	IIC->Address = ADDRESS_R;
	addr = addr + ADDRESS_T;

	return IIC->Read(addr);
}

bool JTW8953::Write(uint addr, const Buffer& bs) const
{
	if(!IIC) return false;
	
	IIC->Address = ADDRESS_W;

	return IIC->Write((ushort)addr, bs);
}

bool JTW8953::Read(uint addr, Buffer& bs) const
{
	if(!IIC) return false;

	IIC->Address = ADDRESS_R;
	
	uint len = IIC->Read((ushort)addr, bs);
	if(len == 0)return false;
	if(len != bs.Length()) bs.SetLength(len);

	return true;
}
