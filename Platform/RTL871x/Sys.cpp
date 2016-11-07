#include "Kernel\Sys.h"

#include "RTL871x.h"

extern uint __heap_base;
extern uint __heap_limit;
extern uint __initial_sp;
extern uint __microlib_freelist;
extern uint __microlib_freelist_initialised;

typedef struct
{
  __IO uint32_t IDCODE;
  __IO uint32_t CR;	
}DBGMCU_TypeDef;
#define DBGMCU_BASE          ((uint32_t)0xE0042000) /*!< Debug MCU registers base address */
#define DBGMCU              ((DBGMCU_TypeDef *) DBGMCU_BASE)

void TSys::OnInit()
{
    Clock	= 83000000;
    CystalClock	= 12000000;    // 晶振时钟
    MessagePort	= COM1; // COM1;

	void* p	= (void*)0x1FFFF7AC;	// 手册里搜索UID，优先英文手册
	Buffer::Copy(ID, p, ArrayLength(ID));

    CPUID = SCB->CPUID;
    uint mcuid = DBGMCU->IDCODE; // MCU编码。低字设备版本，高字子版本
	RevID = mcuid >> 16;
	DevID = mcuid & 0x0FFF;

    FlashSize	= 0x1000000;  // 容量
	RAMSize	= 0x800000;
}

void TSys::InitClock()
{
}

// 堆起始地址，前面是静态分配内存
uint TSys::HeapBase() const
{
	return (uint)&__heap_base;
}

// 栈顶，后面是初始化不清零区域
uint TSys::StackTop() const
{
	return 0x1000000 + (RAMSize << 10) - 0x40;
}

void TSys::SetStackTop(uint addr)
{
	__set_MSP(addr);
}

#if !defined(TINY) && defined(STM32F0) && defined(DEBUG)
	#pragma arm section code = "SectionForSys"
#endif

bool TSys::CheckMemory() const
{
#if DEBUG
	uint msp = __get_MSP();

	//if(__microlib_freelist >= msp) return false;
	assert(__microlib_freelist + 0x40 < msp, "堆栈相互穿透，内存已用完！可能是堆分配或野指针带来了内存泄漏！");

	// 如果堆只剩下64字节，则报告失败，要求用户扩大堆空间以免不测
	//uint end = SRAM_BASE + (RAMSize << 10);
	//if(__microlib_freelist + 0x40 >= end) return false;
	assert(__microlib_freelist + 0x40 < SRAM_BASE + (RAMSize << 10), "堆栈相互穿透，内存已用完！一定是堆分配带来了内存泄漏！");
#endif

	return true;
}

#pragma arm section code

#if DEBUG
typedef struct
{
	byte Revision:4;	// The p value in the Rnpn product revision identifier, indicates patch release.0x0: patch 0
	ushort PartNo:12;	// Part number of the processor. 0xC20: Cortex-M0
	byte Constant:4;	// Constant that defines the architecture of the processor. 0xC: ARMv6-M architecture
	byte Variant:4;		// Variant number: The r value in the Rnpn product revision identifier. 0x0: revision 0
	byte Implementer;	// Implementer code. 0x41 ARM
}ST_CPUID;
#endif

void TSys::OnShowInfo() const
{
#if DEBUG
	debug_printf("SmartOS::");
    bool IsGD = Get_JTAG_ID() == 0x7A3;
	if(IsGD)
		debug_printf("GD32");
	else
		debug_printf("STM32");

	auto cpu = (ST_CPUID*)&CPUID;
	if(DevID > 0)
	{
		if(DevID == 0x410)
		{
			if(IsGD && RevID == 0x1303)
			{
				if(Clock == 48000000)
					debug_printf("F130");
				else
					debug_printf("F150");
			}
#ifdef STM32F1
			else
				debug_printf("F103");
#endif
		}
#ifdef STM32F1
		else if(DevID == 0x412 || DevID == 0x414 || DevID == 0x430)
			debug_printf("F103");
		else if(DevID == 0x418)
			debug_printf("F107");
		else if(DevID == 0x412)
			debug_printf("F130");
#endif
#ifdef STM32F4
		else if(DevID == 0x413)
			debug_printf("F407");
#endif
		else if(DevID == 0x440 || DevID == 0x444) // F030x4/F030x6=0x444 F030x8=0x440
			debug_printf("F030/F051");
		else
			debug_printf("F%03x", DevID);
	}
	else if(CPUID > 0)
	{
		if(Clock == 48000000)
			debug_printf("F130/F150");
#ifdef STM32F1
		else
			debug_printf("F103");
#endif
	}

	// 暂时不知道怎么计算引脚，一般F4/F6/C8CB/RB/VC/VE
	if(_Index < 2)
		debug_printf("F");
	else if(_Index < 4)
		debug_printf("C");
	else if(_Index < 6)
		debug_printf("R");
	else
		debug_printf("V");
	debug_printf("%c", MemNames[_Index]);
	//debug_printf("\r\n");

    // 系统信息
    //debug_printf(" %dMHz Flash:%dk RAM:%dk\r\n", Clock/1000000, FlashSize, RAMSize);
    // 获取当前频率
    debug_printf(" %dMHz Flash:%dk RAM:%dk\r\n", RCC_GetSysClock()/1000000, FlashSize, RAMSize);
	//debug_printf("\r\n");
    debug_printf("DevID:0x%04X RevID:0x%04X \r\n", DevID, RevID);

    debug_printf("CPUID:0x%08X", CPUID);
	if(cpu->Implementer == 0x41) debug_printf(" ARM:");
	if(cpu->Constant == 0x0C)
		debug_printf(" ARMv6-M");
	else if(cpu->Constant == 0x0F)
		debug_printf(" ARMv7-M");
	if((cpu->PartNo & 0x0FF0) == 0x0C20) debug_printf(" Cortex-M%d:", cpu->PartNo & 0x0F);
	debug_printf(" R%dp%d", cpu->Revision, cpu->Variant);
    debug_printf("\r\n");

	// 输出堆信息
	uint start	= (uint)&__heap_base;
	// F4有64k的CCM内存
#if defined(STM32F4)
	if(start < 0x20000000) start = 0x20000000;
#endif
	uint end	= SRAM_BASE + (RAMSize << 10);
	uint size	= end - start;
	debug_printf("Heap :(0x%08x, 0x%08x) = 0x%x (%dk)\r\n", start, end, size, size >> 10);
	start = (uint)&__heap_limit;
#if defined(STM32F4)
	if(start < 0x20000000) start = 0x20000000;
#endif
	//end = 0x20000000 + (RAMSize << 10);
	size	= end - start;
	debug_printf("Stack:(0x%08x, 0x%08x) = 0x%x (%dk)\r\n", start, end, size, size >> 10);

	if(IsGD) debug_printf("ChipType:0x%08X %s\r\n", *(uint*)0x40022100, (cstring)0x40022100);
#endif
}

void TSys::Reset() const { NVIC_SystemReset(); }

void TSys::OnStart()
{
#if !DEBUG
	//WatchDog::Start();
#endif
}

/******************************** 临界区 ********************************/

void EnterCritical()	{ __disable_irq(); }
void ExitCritical()		{ __enable_irq(); }

/******************************** REV ********************************/

uint	_REV(uint value)		{ return __REV(value); }
ushort	_REV16(ushort value)	{ return __REV16(value); }
