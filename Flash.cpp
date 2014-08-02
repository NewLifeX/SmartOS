#include "Flash.h"
#include <stdlib.h>

#define FLASH_DEBUG 1

static const uint STM32_FLASH_KEY1 = 0x45670123;
static const uint STM32_FLASH_KEY2 = 0xcdef89ab;

#if defined (STM32F10X_HD) || defined (STM32F10X_HD_VL) || defined (STM32F10X_CL) || defined (STM32F10X_XL)
  #define FLASH_PAGE_SIZE    ((uint16_t)0x800)
#else
  #define FLASH_PAGE_SIZE    ((uint16_t)0x400)
#endif

Flash::Flash()
{
	Size = *(__IO ushort *)(0x1FFFF7E0) << 10;  // 里面的单位是k，要转为字节
	BytesPerBlock = FLASH_PAGE_SIZE;
    StartAddress = 0x8000000;
}

/* 读取段数据 （起始段，字节数量，目标缓冲区） */
bool Flash::Read(uint address, byte* buf, uint numBytes)
{
    if (buf == NULL) return false;
    if(address < StartAddress || address + numBytes > StartAddress + Size) return false;

#if FLASH_DEBUG
    printf( "Flash::Read( 0x%08x, %d, 0x%08x)\r\n", address, numBytes, buf );
#endif

	// 按字读取
    ushort* ChipAddress = (ushort *)address;
    ushort* EndAddress  = (ushort *)(address + numBytes);
    ushort* pBuf        = (ushort *)buf;

    while(ChipAddress < EndAddress)
    {
        *pBuf++ = *ChipAddress++;
    }

    return true;
}

/* 写入段数据 （起始段，段数量，目标缓冲区，读改写） */
bool Flash::WriteBlock(uint address, byte* buf, uint numBytes, bool fIncrementDataPtr)
{
    if(address < StartAddress || address + numBytes > StartAddress + Size) return false;

    // 进行闪存编程操作时(写或擦除)，必须打开内部的RC振荡器(HSI)
    // 打开 HSI 时钟
    RCC->CR |= RCC_CR_HSION;
    while(!(RCC->CR & RCC_CR_HSIRDY));

    if (FLASH->CR & FLASH_CR_LOCK) { // unlock
        FLASH->KEYR = STM32_FLASH_KEY1;
        FLASH->KEYR = STM32_FLASH_KEY2;
    }
    //FLASH_Unlock();

    ushort* ChipAddress = (ushort *)address;
    ushort* EndAddress  = (ushort *)(address + numBytes);
    ushort* pBuf        = (ushort *)buf;

    // 开始编程
    FLASH->CR = FLASH_CR_PG;
    //FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

    while(ChipAddress < EndAddress) {
        if (*ChipAddress != *pBuf) {
            // write data
            *ChipAddress = *pBuf;
            // wait for completion
            while (FLASH->SR & FLASH_SR_BSY);
            // check
            if (*ChipAddress != *pBuf) {
                printf( " Flash_WriteToSector failure @ 0x%08x, write 0x%08x, read 0x%08x\r\n", (uint)ChipAddress, *pBuf, *ChipAddress );
                return false;
            }
        }
        ChipAddress++;
        if(fIncrementDataPtr) pBuf++;
    }

    // 重置并锁定控制器。直接赋值一了百了，后面两个都是位运算，更麻烦
    FLASH->CR = FLASH_CR_LOCK;
    //FLASH->CR &= ~FLASH_CR_PG;
    //FLASH_Lock();

    // 关闭 HSI 时钟
    RCC->CR &= ~RCC_CR_HSION;

    return true;
}

bool Flash::Write(uint address, byte* buf, uint numBytes, bool readModifyWrite)
{
    if(address < StartAddress || address + numBytes > StartAddress + Size) return false;

#if FLASH_DEBUG
    printf( "Flash::Write( 0x%08x, %d, 0x%08x, %d )", address, numBytes, buf, readModifyWrite );
    int len = 0x10;
    if(numBytes < len) len = numBytes;
    //printf("    Data: ");
    //!!! 必须另起一个指针，否则移动原来的指针可能造成重大失误
    byte* p = buf;
    for(int i=0; i<len; i++) printf(" %02X", *p++);
    printf("\r\n");
#endif

    // 如果没有读改写，直接执行
    if(!readModifyWrite) return WriteBlock(address, buf, numBytes, true);

    // Flash操作一般需要先擦除然后再写入，由于擦除是按块进行，这就需要先读取出来，修改以后再写回去

    // 从地址找到所在扇区
    uint  offset          = (uint)address % BytesPerBlock;
    uint  addr            = address;
    uint  addrEnd         = address + numBytes;
    uint  index           = 0;
    uint  startBlock      = 0;

    // 分配一块内存，作为读取备份修改使用
    byte* pData;
    byte* pBuf = (byte*)malloc(BytesPerBlock);
    if(pBuf == NULL) return false;

    // 循环处理所有需要写入的数据
    bool fRet = true;
    while(fRet && addr < addrEnd)
    {
        // 地址所在区块头部
        startBlock = addr - offset;
        // 如果位移为0，并且整体要写入的字节数大于等于每一块大小
        if(offset == 0 && numBytes >= BytesPerBlock)
        {
            // 则直接使用原始地址
            pData = &buf[index];
        }
        else
        {
            // 否则，把原始地址的数据拷贝到临时内存
            int bytes = BytesPerBlock - offset;
            if(numBytes < bytes) bytes = numBytes;

            // 整块内存原始数据拷贝到临时内存
            memcpy(&pBuf[0], (void*)startBlock, BytesPerBlock);
            // 拷贝需要写入的目标数据到临时内存，起到修改作用
            memcpy(&pBuf[offset], &buf[index], bytes);

            pData = pBuf;
        }

        // 整块擦除内存区，先检查是否需要擦除，这样性能更好。擦除比较慢
        if(!IsBlockErased( startBlock, BytesPerBlock ) && !EraseBlock( startBlock ))
        {
            // 不能直接返回，稍候需要释放内存
            fRet = false;
            break;
        }

        // 写入数据
        fRet = WriteBlock(startBlock, pData, BytesPerBlock, true);

        // 重新定位偏移量
        numBytes -= BytesPerBlock;
        addr     += BytesPerBlock;
        index    += BytesPerBlock - offset;
        offset    = 0;
    }

    if(pBuf != NULL) free(pBuf);

    return fRet;
}

bool Flash::Memset(uint address, byte data, uint numBytes)
{
    if(address < StartAddress || address + numBytes > StartAddress + Size) return false;

#if FLASH_DEBUG
    printf( "Flash::Memset( 0x%08x, %d, 0x%02x )\r\n", address, numBytes, data );
#endif

	// 这里还没有考虑超过一块的情况，将来补上
    return WriteBlock( address, &data, numBytes, false);
}

/* 指定块是否被擦除 */
bool Flash::IsBlockErased(uint address, uint numBytes)
{
    if(address < StartAddress || address + numBytes > StartAddress + Size) return false;

#if FLASH_DEBUG
	printf( "Flash::IsBlockErased( 0x%08x, %d )\r\n", address, numBytes );
#endif

    ushort* ChipAddress = (ushort *) address;
    ushort* EndAddress  = (ushort *)(address + numBytes);

    while(ChipAddress < EndAddress)
    {
        if(*ChipAddress != 0xFFFF)
        {
            return false;
        }
        ChipAddress++;
    }

    return true;
}

/* 擦除块 （段地址） */
bool Flash::EraseBlock(uint address)
{
    if(address < StartAddress || address + BytesPerBlock > StartAddress + Size) return false;

#if FLASH_DEBUG
    printf( "Flash::EraseBlock( 0x%08x )", address );
#endif

    // 进行闪存编程操作时(写或擦除)，必须打开内部的RC振荡器(HSI)
    // 打开 HSI 时钟
    RCC->CR |= RCC_CR_HSION;
    while(!(RCC->CR & RCC_CR_HSIRDY));

    if (FLASH->CR & FLASH_CR_LOCK) { // unlock
        FLASH->KEYR = STM32_FLASH_KEY1;
        FLASH->KEYR = STM32_FLASH_KEY2;
    }
    //FLASH_Unlock();

    // 打开擦除
    FLASH->CR = FLASH_CR_PER;
    // 设置页地址
    FLASH->AR = (uint)address;
    // 开始擦除
    FLASH->CR = FLASH_CR_PER | FLASH_CR_STRT;
    // 确保繁忙标记位被设置 (参考 STM32 勘误表)
    FLASH->CR = FLASH_CR_PER | FLASH_CR_STRT;
    // 等待完成
    while (FLASH->SR & FLASH_SR_BSY);

    //FLASH_Status status = FLASH_COMPLETE;
    //boolret = true;

    /*FLASH_Status status = FLASH_ErasePage(sector);
    boolret = false;
    if(status == FLASH_COMPLETE) ret = true;*/

    // 重置并锁定控制器。直接赋值一了百了，后面两个都是位运算，更麻烦
    FLASH->CR = FLASH_CR_LOCK;
    //FLASH->CR &= ~FLASH_CR_PER;
    //FLASH_Lock();

    // 关闭 HSI 时钟
    RCC->CR &= ~RCC_CR_HSION;

#if FLASH_DEBUG
    byte* p = (byte*)address;
    for(int i=0; i<0x10; i++) printf(" %02X", *p++);
    printf("\r\n");
#endif

    return true;
}

// 擦除块。其实地址，字节数量默认0表示擦除全部
bool Flash::Erase(uint address, uint numBytes)
{
    if(address < StartAddress || address + numBytes > StartAddress + Size) return false;

#if FLASH_DEBUG
    printf( "Flash::Erase( 0x%08x, 0x%08x )", address, numBytes );
#endif

	if(numBytes == 0) numBytes = StartAddress + Size - address;

	// 该地址所在的块
	uint addr = address - ((uint)address % BytesPerBlock);
	uint end  = address + numBytes;
	while(addr < end)
	{
		EraseBlock(addr);
		
		addr += BytesPerBlock;
	}

	return true;
}
