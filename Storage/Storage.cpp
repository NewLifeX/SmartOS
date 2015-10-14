#include "Config.h"

#define STORAGE_DEBUG DEBUG
//#define STORAGE_DEBUG 0

/* 读取段数据 （起始段，字节数量，目标缓冲区） */
bool BlockStorage::Read(uint address, ByteArray& bs)
{
	uint len = bs.Length();
    if (!len) return false;
    if(address < Start || address + len > Start + Size) return false;

#if STORAGE_DEBUG
    debug_printf("BlockStorage::Read(0x%08x, %d, 0x%08x)\r\n", address, len, bs.GetBuffer());
#endif

	bs.Copy((byte*)address);

    return true;
}

bool BlockStorage::Write(uint address, const ByteArray& bs)
{
	uint len = bs.Length();
    if (!len) return false;
    if(address < Start || address + len > Start + Size) return false;

#if STORAGE_DEBUG
    debug_printf("BlockStorage::Write(0x%08x, %d, 0x%08x, %d)", address, len, bs.GetBuffer());
    int len2 = 0x10;
    if(len < len2) len2 = len;
    //debug_printf("    Data: ");
    //!!! 必须另起一个指针，否则移动原来的指针可能造成重大失误
    byte* p = bs.GetBuffer();
    for(int i=0; i<len2; i++) debug_printf(" %02X", *p++);
    debug_printf("\r\n");
#endif

    // 如果没有读改写，直接执行
    if(!ReadModifyWrite) return WriteBlock(address, bs.GetBuffer(), len, true);

    // Flash操作一般需要先擦除然后再写入，由于擦除是按块进行，这就需要先读取出来，修改以后再写回去

    // 从地址找到所在扇区
    uint  offset		= (uint)address % Block;
	byte* pData 		= bs.GetBuffer();					// 指向要下一个写入的数据
	uint remainSize 	= len;		// 剩下数据数量
	uint addressNow 	= address;		// 下一个要写入数据的位置

    // 分配一块内存，作为读取备份修改使用
    //byte* pBuf = (byte*)malloc(Block);
    //if(pBuf == NULL) return false;
#ifdef STM32F0
	byte bb[0x400];
#else
	byte bb[0x800];
#endif
	ByteArray bf;
	bf.Set(bb, ArrayLength(bb));
	bf.SetLength(Block);

	// 写入第一个半块
	if(offset)
	{
		//memcpy(pBuf,(byte *)(addressNow-offset),offset);
		//memcpy(pBuf+offset,pData,Block-offset);
		// 计算块起始地址和剩余大小
		uint addr = addressNow - offset;
		uint size = Block - offset;
		// 先读取偏移之前的原始数据，再从来源数据拷贝半块数据，凑够一块
		bf.Copy((byte*)addr, offset);
		bf.Copy(pData, size, offset);

		// 整块擦除，然后整体写入
		Erase(addr, Block);
		if(!WriteBlock(addr, bf.GetBuffer(), Block, true)) return false;

		remainSize -= size;
		addressNow += size;
		pData += size;
	}

	// 连续整块
	for(int i = 0; remainSize > Block; i++)
	{
		Erase(addressNow, Block);
		if(!WriteBlock(addressNow, pData, Block, true)) return false;

		pData += Block;
		addressNow += Block;
		remainSize -= Block;
	}

	// 最后一个半块
	if(remainSize != 0)
	{
		//memcpy(pBuf, pData, remainSize);
		//memcpy(&pBuf[remainSize], (byte *)(addressNow+remainSize), Block-remainSize);
		// 从来源数据拷贝半块数据，再读取半块原始数据，凑够一块
		bf.Copy(pData, remainSize);
		bf.Copy((byte*)(addressNow + remainSize), Block - remainSize, remainSize);

		Erase(addressNow, Block);
		if(!WriteBlock(addressNow, bf.GetBuffer(), Block, true)) return false;
	}

    return true;
}

bool BlockStorage::Memset(uint address, byte data, uint len)
{
    if(address < Start || address + len > Start + Size) return false;

#if STORAGE_DEBUG
    debug_printf("BlockStorage::Memset(0x%08x, %d, 0x%02x)\r\n", address, len, data);
#endif

	// 这里还没有考虑超过一块的情况，将来补上
    return WriteBlock(address, &data, len, false);
}

// 擦除块。起始地址，字节数量默认0表示擦除全部
bool BlockStorage::Erase(uint address, uint len)
{
    if(address < Start || address + len > Start + Size) return false;

#if STORAGE_DEBUG
    debug_printf("BlockStorage::Erase(0x%08x, 0x%08x)\r\n", address, len);
#endif

	if(len == 0) len = Start + Size - address;

	// 该地址所在的块
	uint addr = address - ((uint)address % Block);
	uint end  = address + len;
	// 需要检查是否擦除的范围，从第一段开始
	uint addr2	= address;
	uint len2	= addr + Block - address;
	// 如果还不够一段，则直接长度
	if(len2 > len) len2 = len;
	while(addr < end)
	{
		if(!IsErased(addr2, len2)) EraseBlock(addr);

		addr	+= Block;
		// 下一段肯定紧跟着人家开始
		addr2	= addr;
		len2	= end - addr;
		if(len2 > Block) len2 = Block;
	}

	return true;
}

/* 指定块是否被擦除 */
bool BlockStorage::IsErased(uint address, uint len)
{
    if(address < Start || address + len > Start + Size) return false;

#if STORAGE_DEBUG
	//debug_printf("BlockStorage::IsErased(0x%08x, %d)\r\n", address, len);
#endif

    ushort* p	= (ushort*)address;
    ushort* e	= (ushort*)(address + len);

    while(p < e)
    {
        if(*p++ != 0xFFFF) return false;
    }

    return true;
}
