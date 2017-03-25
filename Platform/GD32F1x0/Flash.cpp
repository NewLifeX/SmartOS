#include "Device\Flash.h"

#include "Platform\stm32.h"

#define FLASH_DEBUG DEBUG

//static const uint STM32_FLASH_KEY1 = 0x45670123;
//static const uint STM32_FLASH_KEY2 = 0xcdef89ab;

#if defined (STM32F10X_HD) || defined (STM32F10X_HD_VL) || defined (STM32F10X_CL) || defined (STM32F10X_XL)
  #define FLASH_PAGE_SIZE    ((uint16_t)0x800)
#else
  #define FLASH_PAGE_SIZE    ((uint16_t)0x400)
#endif

void Flash::OnInit()
{
	Block	= FLASH_PAGE_SIZE;
}

/* 写入段数据 （起始段，段数量，目标缓冲区，读改写） */
bool Flash::WriteBlock(uint address, const byte* buf, int len, bool inc) const
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
                debug_printf("Flash::WriteBlock 失败 %p, 写 0x%04x, 读 0x%04x\r\n", s, *p, *s);
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
    debug_printf("\tFlash::EraseBlock(%p)\r\n", address);
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




	*     @arg OB_RDP_Level_0: No protection
	*     @arg OB_RDP_Level_1: Read protection of the memory
	*     @arg OB_RDP_Level_2: Chip protection
	* @retval FLASH Status: The returned value can be:
	*         FLASH_ERROR_PROGRAM, FLASH_ERROR_WRP, FLASH_COMPLETE or FLASH_TIMEOUT.

FLASH_Status FLASH_OB_RDPConfig(uint8_t OB_RDP)







}*/


bool Flash::ReadOutProtection(bool set)
{
#if defined(STM32F1)
	bool isProt = FLASH_GetReadOutProtectionStatus() == SET ? true : false;
	if (isProt == set)return isProt;
	if (set)
	{
		// FLASH_Unlock();   // 多处资料都写了这一行并注释了
		FLASH_ReadOutProtection(ENABLE);
	}
	else
	{
		// 取消读保护会清空 Flash 内容，注意：要上电复位才可以使用IC
		FLASH_Unlock();
		FLASH_ReadOutProtection(DISABLE);
	}return set;
#endif
#if defined(STM32F0)
	bool isProt = FLASH_OB_GetRDP() == SET ? true : false;

	if (isProt == set)return isProt;
	if (set)
	{
		FLASH_OB_Unlock();
		FLASH_OB_RDPConfig(OB_RDP_Level_1);
		// FLASH_OB_RDPConfig(OB_RDP_Level_2);

		/*	Warning: When enabling read protection level 2
			it's no more possible to go back to level 1 or 0
		* OB_RDP_Level_0: No protection
		* OB_RDP_Level_1 : Read protection of the memory
		* OB_RDP_Level_2 : Chip protection	*/
		FLASH_OB_Lock();
	}else
	{
		// 取消读保护会清空 Flash 内容，注意：要上电复位才可以使用IC
		FLASH_Unlock();
		FLASH_OB_RDPConfig(OB_RDP_Level_0);
		FLASH_OB_Lock();
	}return set;
#endif
}
