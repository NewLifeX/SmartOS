#include "Flash.h"
#include <stdlib.h>

#define FLASH_DEBUG DEBUG

//static const uint STM32_FLASH_KEY1 = 0x45670123;
//static const uint STM32_FLASH_KEY2 = 0xcdef89ab;

#if defined (STM32F10X_HD) || defined (STM32F10X_HD_VL) || defined (STM32F10X_CL) || defined (STM32F10X_XL)
  #define FLASH_PAGE_SIZE    ((uint16_t)0x800)
#else
  #define FLASH_PAGE_SIZE    ((uint16_t)0x400)
#endif

Flash::Flash()
{
	//Size = *(__IO ushort *)(0x1FFFF7E0) << 10;  // 里面的单位是k，要转为字节
	Size	= Sys.FlashSize << 20;
	Block	= FLASH_PAGE_SIZE;
    Start	= 0x8000000;
}

/* 读取段数据 （起始段，字节数量，目标缓冲区） */
bool Flash::Read(uint address, ByteArray& bs)
{
	uint len = bs.Length();
    if (!len) return false;
    if(address < Start || address + len > Start + Size) return false;

#if FLASH_DEBUG
    debug_printf("Flash::Read( 0x%08x, %d, 0x%08x)\r\n", address, len, bs.GetBuffer());
#endif

	bs.Copy((byte*)address);

    return true;
}

/* 写入段数据 （起始段，段数量，目标缓冲区，读改写） */
bool Flash::WriteBlock(uint address, const byte* buf, uint len, bool inc)
{
    if(address < Start || address + len > Start + Size) return false;

    // 进行闪存编程操作时(写或擦除)，必须打开内部的RC振荡器(HSI)
    // 打开 HSI 时钟
    RCC->CR |= RCC_CR_HSION;
    while(!(RCC->CR & RCC_CR_HSIRDY));

    /*if (FLASH->CR & FLASH_CR_LOCK) { // unlock
        FLASH->KEYR = STM32_FLASH_KEY1;
        FLASH->KEYR = STM32_FLASH_KEY2;
    }*/
    FLASH_Unlock();

    ushort* s	= (ushort*)address;
    ushort* e	= (ushort*)(address + len);
    const ushort* p	= (const ushort*)buf;

    // 开始编程
    FLASH->CR = FLASH_CR_PG;
    //FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

    while(s < e) {
        if (*s != *p) {
            *s = *p;
            while (FLASH->SR & FLASH_SR_BSY);
            if (*s != *p) {
                debug_printf("Flash::WriteBlock 失败 0x%08x, 写 0x%04x, 读 0x%04x\r\n", s, *p, *s);
                return false;
            }
        }
        s++;
        if(inc) p++;
    }

    // 重置并锁定控制器。直接赋值一了百了，后面两个都是位运算，更麻烦
    //FLASH->CR = FLASH_CR_LOCK;
    //FLASH->CR &= ~FLASH_CR_PG;
    FLASH_Lock();

    // 关闭 HSI 时钟
    RCC->CR &= ~RCC_CR_HSION;

    return true;
}

bool Flash::Write(uint address, const ByteArray& bs, bool readModifyWrite)
{
	uint len = bs.Length();
    if (!len) return false;
    if(address < Start || address + len > Start + Size) return false;

#if FLASH_DEBUG
    debug_printf("Flash::Write(0x%08x, %d, 0x%08x, %d)", address, len, bs.GetBuffer(), readModifyWrite);
    int len2 = 0x10;
    if(len < len2) len2 = len;
    //debug_printf("    Data: ");
    //!!! 必须另起一个指针，否则移动原来的指针可能造成重大失误
    byte* p = bs.GetBuffer();
    for(int i=0; i<len2; i++) debug_printf(" %02X", *p++);
    debug_printf("\r\n");
#endif

    // 如果没有读改写，直接执行
    if(!readModifyWrite) return WriteBlock(address, bs.GetBuffer(), len, true);

    // Flash操作一般需要先擦除然后再写入，由于擦除是按块进行，这就需要先读取出来，修改以后再写回去

    // 从地址找到所在扇区
    uint  offset		= (uint)address % Block;
	byte* pData 		= bs.GetBuffer();					// 指向要下一个写入的数据
	uint remainSize 	= len;		// 剩下数据数量
	uint addressNow 	= address;		// 下一个要写入数据的位置

    // 分配一块内存，作为读取备份修改使用
    //byte* pBuf = (byte*)malloc(Block);
    //if(pBuf == NULL) return false;
	byte bb[FLASH_PAGE_SIZE];
	ByteArray bf;
	bf.Set(bb, FLASH_PAGE_SIZE);
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

bool Flash::Memset(uint address, byte data, uint numBytes)
{
    if(address < Start || address + numBytes > Start + Size) return false;

#if FLASH_DEBUG
    debug_printf("Flash::Memset( 0x%08x, %d, 0x%02x )\r\n", address, numBytes, data);
#endif

	// 这里还没有考虑超过一块的情况，将来补上
    return WriteBlock( address, &data, numBytes, false);
}

/* 指定块是否被擦除 */
bool Flash::IsBlockErased(uint address, uint numBytes)
{
    if(address < Start || address + numBytes > Start + Size) return false;

#if FLASH_DEBUG
	debug_printf( "Flash::IsBlockErased( 0x%08x, %d )\r\n", address, numBytes );
#endif

    ushort* p	= (ushort*)address;
    ushort* e	= (ushort*)(address + numBytes);

    while(p < e)
    {
        if(*p++ != 0xFFFF) return false;
    }

    return true;
}

/* 擦除块 （段地址） */
bool Flash::EraseBlock(uint address)
{
    if(address < Start || address + Block > Start + Size) return false;

#if FLASH_DEBUG
    debug_printf("\tFlash::EraseBlock(0x%08x)\r\n", address);
#endif

    // 进行闪存编程操作时(写或擦除)，必须打开内部的RC振荡器(HSI)
    // 打开 HSI 时钟
    RCC->CR |= RCC_CR_HSION;
    while(!(RCC->CR & RCC_CR_HSIRDY));

    /*if (FLASH->CR & FLASH_CR_LOCK) { // unlock
        FLASH->KEYR = STM32_FLASH_KEY1;
        FLASH->KEYR = STM32_FLASH_KEY2;
    }*/
    FLASH_Unlock();

    // 打开擦除
    /*FLASH->CR = FLASH_CR_PER;
    // 设置页地址
    FLASH->AR = (uint)address;
    // 开始擦除
    FLASH->CR = FLASH_CR_PER | FLASH_CR_STRT;
    // 确保繁忙标记位被设置 (参考 STM32 勘误表)
    FLASH->CR = FLASH_CR_PER | FLASH_CR_STRT;
    // 等待完成
    while (FLASH->SR & FLASH_SR_BSY);*/

    //FLASH_Status status = FLASH_COMPLETE;
    //boolret = true;

#ifndef STM32F4
    FLASH_Status status = FLASH_ErasePage(address);
#else
    FLASH_Status status = FLASH_EraseSector(address, VoltageRange_3);
#endif
    bool ret = status == FLASH_COMPLETE;

    // 重置并锁定控制器。直接赋值一了百了，后面两个都是位运算，更麻烦
    //FLASH->CR = FLASH_CR_LOCK;
    //FLASH->CR &= ~FLASH_CR_PER;
    FLASH_Lock();

    // 关闭 HSI 时钟
    RCC->CR &= ~RCC_CR_HSION;

#if FLASH_DEBUG
    /*byte* p = (byte*)address;
    for(int i=0; i<0x10; i++) debug_printf(" %02X", *p++);
    debug_printf("\r\n");*/
#endif

    return ret;
}

// 擦除块。起始地址，字节数量默认0表示擦除全部
bool Flash::Erase(uint address, uint numBytes)
{
    if(address < Start || address + numBytes > Start + Size) return false;

#if FLASH_DEBUG
    debug_printf("Flash::Erase(0x%08x, 0x%08x)\r\n", address, numBytes);
#endif

	if(numBytes == 0) numBytes = Start + Size - address;

	// 该地址所在的块
	uint addr = address - ((uint)address % Block);
	uint end  = address + numBytes;
	while(addr < end)
	{
		EraseBlock(addr);

		addr += Block;
	}

	return true;
}
