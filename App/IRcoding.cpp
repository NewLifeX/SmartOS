
#include "IRcoding.h"

ushort IRcoding::CodingSize = 255; 

IRcoding::IRcoding(Storage* flash, uint blockSize)
{
	assert(flash, "必须给出存储介质");
	assert(blockSize, "必须给出具体块大小");
	_Medium = flash;
	_BlockSize = blockSize;
	// 计算需要多少个 Block 存储一个 编码
	for(int i = 1; i < 0xff; i++)
	{
		if(i*blockSize > CodingSize)
		{
			_CodingBlock = i;
			break;
		}
	}
}

//IRcoding::~IRcoding()
//{
//	
//}

void IRcoding::SetCfgAddr(uint addr)
{
	_OrigAddr = addr;
	// 获取第一个 block 的描述信息，知道有多少编码在存
	ByteArray bs(CodingSize);
	GetCoding(0, bs);
	if(!bs.Length())
	{
		debug_printf("获取配置区失败，创建配置区");
		bs.SetLength(2);
	}
}

bool IRcoding::SaveCoding(byte index, const Buffer& bs)
{
	if(bs.Length() > CodingSize)return false;
	if(!_Medium)return false;
	uint address = _OrigAddr + CodingSize * index;
	return	_Medium->Write(address, bs);
}

bool IRcoding::GetCoding(byte index, Buffer& bs)
{
	if(bs.Length() > CodingSize)return false;
	if(!_Medium)return false;
	
	uint address = _OrigAddr + CodingSize * index;
	bs.SetLength(CodingSize);
	
	bool stat = _Medium->Read(address, bs);
	
	if(!stat)return false;
	bs.SetLength(bs[0]);
	return true;
}
