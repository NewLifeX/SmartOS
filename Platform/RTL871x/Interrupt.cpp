#include "Kernel\Sys.h"

#include "Kernel\Interrupt.h"
//#include "SerialPort.h"

#include "RTL871x.h"

bool TInterrupt::OnActivate(short irq)
{
    __DMB(); // asure table is written
    //NVIC->ICPR[irq >> 5] = 1 << (irq & 0x1F); // clear pending bit
    //NVIC->ISER[irq >> 5] = 1 << (irq & 0x1F); // set enable bit
    if(irq >= 0)
    {
        NVIC_ClearPendingIRQ((IRQn_Type)irq);
        NVIC_EnableIRQ((IRQn_Type)irq);
    }

    return true;
}

bool TInterrupt::OnDeactivate(short irq)
{
    //NVIC->ICER[irq >> 5] = 1 << (irq & 0x1F); // clear enable bit */
    if(irq >= 0) NVIC_DisableIRQ((IRQn_Type)irq);
    return true;
}

void TInterrupt::SetPriority(short irq, uint priority) const
{
    NVIC_SetPriority((IRQn_Type)irq, priority);
}

void TInterrupt::GetPriority(short irq) const
{
    NVIC_GetPriority((IRQn_Type)irq);
}

void TInterrupt::GlobalEnable()	{ __enable_irq(); }
void TInterrupt::GlobalDisable(){ __disable_irq(); }
bool TInterrupt::GlobalState()	{ return __get_PRIMASK(); }

#if !defined(TINY) && defined(STM32F0)
	#pragma arm section code = "SectionForSys"
#endif

#if defined ( __CC_ARM   )
__ASM uint GetIPSR()
{
    mrs     r0,IPSR               // exception number
    bx      lr
}
#elif (defined (__GNUC__))
uint32_t GetIPSR(void) __attribute__( ( naked ) );
uint32_t GetIPSR(void)
{
  uint32_t result=0;

  __ASM volatile ("MRS r0, IPSR\n\t" 
                  "BX  lr     \n\t"  : "=r" (result) );
  return(result);
}
#endif

// 是否在中断里面
bool TInterrupt::IsHandler() { return GetIPSR() & 0x01; }

#ifdef TINY
void FaultHandler() { }
#else
void UserHandler()
{
    uint num = GetIPSR();
	Interrupt.Process(num);
}
#endif

void assert_failed(uint8_t* file, unsigned int line)
{
    debug_printf("Assert Failed! Line %d, %s\r\n", line, file);

	TInterrupt::Halt();
}
