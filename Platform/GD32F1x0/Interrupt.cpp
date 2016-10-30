#include "Sys.h"

#include "Kernel\Interrupt.h"

#include "..\stm32.h"

#define IS_IRQ(irq) (irq >= -16 && irq <= VectorySize - 16)

void TInterrupt::OnInit() const
{
    // 禁用所有中断
    NVIC->ICER[0] = 0xFFFFFFFF;
    NVIC->ICER[1] = 0xFFFFFFFF;
    NVIC->ICER[2] = 0xFFFFFFFF;

    // 清除所有中断位
    NVIC->ICPR[0] = 0xFFFFFFFF;
    NVIC->ICPR[1] = 0xFFFFFFFF;
    NVIC->ICPR[2] = 0xFFFFFFFF;

#if defined(BOOT) || defined(APP)
	StrBoot.pUserHandler = RealHandler;;
#endif

#if defined(APP)
	GlobalEnable();
#endif

	// 初始化EXTI中断线为默认值
	EXTI_DeInit();
}

#if !defined(TINY)
#if defined(STM32F0)
	#pragma arm section code = "SectionForSys"
#endif

extern "C"
{
	/*  在GD32F130C8中时常有一个FAULT_SubHandler跳转的错误，
	经查是因为FAULT_SubHandler和FaultHandler离得太远，导致b跳转无法到达，
	因此把这两个函数指定到同一个段中，使得它们地址分布上紧挨着
	*/
	void FAULT_SubHandler(uint* registers, uint exception)
	{
		debug_printf("LR=0x%08x PC=0x%08x PSR=0x%08x SP=0x%08x\r\n", registers[13], registers[14], registers[15], registers[16]);
		for(int i=0; i<=12; i++)
		{
			debug_printf("R%d=0x%08x\r\n", i, registers[i]);
		}

		TInterrupt::Halt();
	}

#if defined ( __CC_ARM   )
	__asm void FaultHandler()
	{
		IMPORT FAULT_SubHandler

		//on entry, we have an exception frame on the stack:
		//SP+00: R0
		//SP+04: R1
		//SP+08: R2
		//SP+12: R3
		//SP+16: R12
		//SP+20: LR
		//SP+24: PC
		//SP+28: PSR
		//R0-R12 are not overwritten yet
		add      sp,sp,#16             // remove R0-R3
		push     {r0-r11}              // store R0-R11
		mov      r0,sp
		//R0+00: R0-R12
		//R0+52: LR
		//R0+56: PC
		//R0+60: PSR
		mrs      r1,IPSR               // exception number
		b        FAULT_SubHandler
		//never expect to return
	}
#elif (defined (__GNUC__))
	void FaultHandler()
	{
		__asm volatile (
			"add	sp,sp,#16	\n\t"
			"push	{r0-r11}	\n\t"
			"mov	r0,sp	\n\t"
			"mrs	r1,IPSR	\n\t"
			"b	FAULT_SubHandler	\n\t");
	}
#endif
}
#endif
