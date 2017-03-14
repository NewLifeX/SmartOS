#include "Storage.h"

//#define STORAGE_DEBUG DEBUG
#define STORAGE_DEBUG 0
#if STORAGE_DEBUG
	#define st_printf debug_printf
#else
	#define st_printf(format, ...)
#endif

WEAK bool ReadBlock(uint address, Buffer& bs)
{
	return bs.Copy(0, (byte*)address, -1);
}

bool BlockStorage::Read(uint address, Buffer& bs) const
{
	int len = bs.Length();
    if (!len) return true;

    if(address < Start || address + len > Start + Size) return false;

#if STORAGE_DEBUG
    st_printf("BlockStorage::Read(%p, %d, %p)\r\n", address, len, bs.GetBuffer());
#endif

	if(XIP)
		bs.Copy(0, (byte*)address, -1);
	else
		ReadBlock(address, bs);

    return true;
}

bool BlockStorage::Write(uint address, const Buffer& bs) const
{
    assert((address & 0x01) == 0x00, "Write起始地址必须是2字节对齐");

	auto buf	= bs.GetBuffer();
	int len	= bs.Length();
    if (!len) return true;

    if(address < Start || address + len > Start + Size) return false;

#if STORAGE_DEBUG
    st_printf("BlockStorage::Write(%p, %d, %p)", address, len, buf);
    int len2 = 0x10;
    if(len < len2) len2 = len;
    //st_printf("    Data: ");
    //!!! 必须另起一个指针，否则移动原来的指针可能造成重大失误
    auto p = buf;
    for(int i=0; i<len2; i++) st_printf(" %02X", *p++);
    st_printf("\r\n");
#endif

	// 长度也必须2字节对齐
	if(len & 0x01) len++;
	// 比较跳过相同数据
	if(XIP)
	{
		while(len)
		{
			if(*(ushort*)address != *(ushort*)buf) break;

			address	+= 2;
			buf		+= 2;
			len		-= 2;
		}
		// 从后面比较相同数据
		while(len)
		{
			if(*(ushort*)(address + len - 2) != *(ushort*)(buf + len - 2)) break;

			len		-= 2;
		}
		if(!len)
		{
			st_printf("数据相同，无需写入！\r\n");
			return true;
		}
	}

    // 如果没有读改写，直接执行
    if(!ReadModifyWrite) return WriteBlock(address, buf, len, true);

    // Flash操作一般需要先擦除然后再写入，由于擦除是按块进行，这就需要先读取出来，修改以后再写回去

    // 从地址找到所在扇区
    int  offset	= address % Block;
	auto pData	= buf;		// 指向要下一个写入的数据
	int remain	= len;		// 剩下数据数量
	int addr	= address;	// 下一个要写入数据的位置

    // 分配一块内存，作为读取备份修改使用
#ifdef STM32F0
	byte bb[0x400];
#else
	byte bb[0x800];
#endif
	Buffer ms(bb, sizeof(bb));
	ms.SetLength(Block);

	// 写入第一个半块
	if(offset)
	{
		// 计算块起始地址和剩余大小
		int blk	= addr - offset;
		int size	= Block - offset;
		// 前段原始数据，中段来源数据，末段原始数据
		ms.Copy(0, (byte*)blk, offset);
		if(size > remain) size = remain;
		ms.Copy(offset, pData, size);

		int	offset2	= offset + size;
		int last	= Block - offset2;
		if(last > 0) ms.Copy(offset2, (byte*)(blk + offset2), last);

		// 整块擦除，然后整体写入
		//if(!IsErased(blk + offset, size)) Erase(blk, Block);
		Erase(blk + offset, size);
		if(!WriteBlock(blk, ms.GetBuffer(), Block, true)) return false;

		remain	-= size;
		addr	+= size;
		pData	+= size;
	}

	// 连续整块
	for(int i = 0; remain > (int)Block; i++)
	{
		Erase(addr, Block);
		if(!WriteBlock(addr, pData, Block, true)) return false;

		remain	-= Block;
		addr	+= Block;
		pData	+= Block;
	}

	// 最后一个半块
	if(remain > 0)
	{
		// 前段来源数据，末段原始数据
		//ms.SetPosition(0);
		ms.Copy(0, pData, remain);
		ms.Copy(remain, (byte*)(addr + remain), Block - remain);

		//if(!IsErased(addr, remain)) Erase(addr, Block);
		Erase(addr, remain);
		if(!WriteBlock(addr, ms.GetBuffer(), Block, true)) return false;
	}

    return true;
}

bool BlockStorage::Memset(uint address, byte data, int len) const
{
    assert((address & 0x01) == 0x00, "Memset起始地址必须是2字节对齐");

    if(address < Start || address + len > Start + Size) return false;

#if STORAGE_DEBUG
    st_printf("BlockStorage::Memset(%p, %d, 0x%02x)\r\n", address, len, data);
#endif

	// 这里还没有考虑超过一块的情况，将来补上
    return WriteBlock(address, &data, len, false);
}

// 擦除块。起始地址，字节数量默认0表示擦除全部
bool BlockStorage::Erase(uint address, int len) const
{
    assert((address & 0x01) == 0x00, "Erase起始地址必须是2字节对齐");

    if(address < Start || address + len > Start + Size) return false;

#if STORAGE_DEBUG
    st_printf("BlockStorage::Erase(%p, %d)\r\n", address, len);
#endif

	if(len == 0) len = Start + Size - address;

	// 该地址所在的块
	uint blk	= address - ((uint)address % Block);
	uint end	= address + len;
	// 需要检查是否擦除的范围，从第一段开始
	uint addr	= address;
	int len2	= blk + Block - address;
	// 如果还不够一段，则直接长度
	if(len2 > len) len2 = len;
	while(blk < end)
	{
		if(!IsErased(addr, len2)) EraseBlock(blk);

		blk		+= Block;
		// 下一段肯定紧跟着人家开始
		addr	= blk;
		len2	= end - blk;
		if(len2 > Block) len2 = Block;
	}

	return true;
}

/* 指定块是否被擦除 */
bool BlockStorage::IsErased(uint address, int len) const
{
    assert((address & 0x01) == 0x00, "IsErased起始地址必须是2字节对齐");

    if(address < Start || address + len > Start + Size) return false;

#if STORAGE_DEBUG
	//st_printf("BlockStorage::IsErased(%p, %d)\r\n", address, len);
#endif

	if(!XIP) return false;

    ushort* p	= (ushort*)address;
    ushort* e	= (ushort*)(address + len);

    while(p < e)
    {
        if(*p++ != 0xFFFF) return false;
    }

    return true;
}

bool CharStorage::Read(uint address, Buffer& bs) const
{
	bs.Copy(0, (byte*)address, -1);

	return true;
}

bool CharStorage::Write(uint address, const Buffer& bs) const
{
	bs.CopyTo(0, (byte*)address, -1);

	return true;
}
