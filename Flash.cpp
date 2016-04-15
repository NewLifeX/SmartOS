#include "Flash.h"

#include "Platform\stm32.h"

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
	Size	= Sys.FlashSize << 10;
	Block	= FLASH_PAGE_SIZE;
    Start	= 0x8000000;
	ReadModifyWrite	= true;
}

/* 写入段数据 （起始段，段数量，目标缓冲区，读改写） */
bool Flash::WriteBlock(uint address, const byte* buf, uint len, bool inc) const
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

/* 擦除块 （段地址） */
bool Flash::EraseBlock(uint address) const
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

/*
// 设置读保护
if(FLASH_GetReadOutProtectionStatus() != SET)
{
	FLASH_Unlock();
	FLASH_ReadOutProtection(ENABLE);
}
// 取消读保护  执行后Flash就清空了  注意：要上电复位才可以使用IC
if(FLASH_GetReadOutProtectionStatus() != RESET)
{
	FLASH_Unlock();
	FLASH_ReadOutProtection(DISABLE);
}*/

bool Flash::ReadOutProtection(bool set)
{
	bool isProt = FLASH_GetReadOutProtectionStatus()== SET ? true:false;
	if (isProt == set)return isProt;
	if (set)
	{
		// FLASH_Unlock();   // 多处资料都写了这一行并注释了
		FLASH_ReadOutProtection(ENABLE);
	}else
	{
		// 取消读保护会清空 Flash 内容，注意：要上电复位才可以使用IC
		FLASH_Unlock();
		FLASH_ReadOutProtection(DISABLE);
	}return set;
}
