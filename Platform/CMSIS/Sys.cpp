#include "Sys.h"

#include "Time.h"
#include "WatchDog.h"

#include "Platform\stm32.h"

#if defined(BOOT) || defined(APP)

//#pragma location  = 0x20000000
struct HandlerRemap StrBoot __attribute__((at(0x2000fff0)));

#endif

extern uint __heap_base;
extern uint __heap_limit;
extern uint __initial_sp;
extern uint __microlib_freelist;
extern uint __microlib_freelist_initialised;

#ifndef BIT
    #define BIT(x)	(1 << (x))
#endif
#ifndef TINY
static int _Index;	// MCU在型号表中的索引
#endif

#ifndef TINY

#ifdef STM32F1
#if DEBUG
	const char MemNames[] = "468BCDEFGIK";
#endif
	const uint MemSizes[] = { 16, 32, 64, 128, 256, 384, 512, 768, 1024, 2048, 3072 };
	const uint RamSizes[] = {  6, 10, 20,  20,  48,  48,  64,  96,   96,   96,   96 };
#elif defined(STM32F0) || defined(GD32F150)
#if DEBUG
	const char MemNames[] = "468B";
#endif
	const uint MemSizes[] = { 16, 32, 64, 128 };
	//uint RamSizes[] = {  4,  6,  8,  16 }; // 150x6有6kRAM
	const uint RamSizes[] = {  4,  4,  8,  16 };
#elif defined(STM32F4)
#if DEBUG
	const char MemNames[] = "468BCDEFGIK";
#endif
	const uint MemSizes[] = { 16, 32, 64, 128, 256, 384, 512, 768, 1024, 2048, 3072 };
	const uint RamSizes[] = {  6, 10, 20,  20,  48,  48, 128, 192,  128,  192,  192 };
#endif

#endif

#if !defined(TINY) && defined(STM32F0)
	#pragma arm section code = "SectionForSys"
#endif

_force_inline void InitHeapStack(uint top)
{
	uint* p = (uint*)__get_MSP();

	uint size = (uint)&__initial_sp - (uint)p;
	uint msp = top - size;
	// 拷贝一部分栈内容到新栈
	Buffer((void*)msp, size)	= (void*)p;

	// 必须先拷贝完成栈，再修改栈指针
	__set_MSP(msp);

	// 这个时候还没有初始化堆，我们来设置堆到内存最大值，让堆栈共用RAM剩下全部
	//__microlib_freelist
	p = &__heap_base + 1;
	if(!__microlib_freelist_initialised)
	{
		//*(uint*)&__heap_limit = top;
		// 堆顶地址__heap_limit被编译固化到malloc函数附近，无法修改。只能这里负责初始化整个堆
		__microlib_freelist = (uint)p;	// 空闲链表指针
		// 设置堆大小，直达天花板
		*p = (top - (uint)p) & 0xFFFFFFF8;	// 空闲链表剩余大小
		*(p + 1) = 0;

		__microlib_freelist_initialised = 1;
	}
	else
	{
		//// 如果已经初始化堆，则直接增加大小即可
		//*p += (top - (uint)&__heap_limit) & 0xFFFFFFF8;	// 空闲链表剩余大小

		// 需要找到最后一个空闲节点，然后把大小加上去
		uint* p = (uint*)__microlib_freelist;
		// 每一个节点由4字节大小以及4字节下一节点的指针组成
		while(*(p + 1)) p = (uint*)*(p + 1);

		// 给最后一个空闲节点增加大小
		*p += (top - (uint)p) & 0xFFFFFFF8;	// 空闲链表剩余大小
	}
}

// 获取JTAG编号，ST是0x041，GD是0x7A3
uint16_t Get_JTAG_ID()
{
    if( *( uint8_t *)( 0xE00FFFE8 ) & 0x08 )
    {
        return  ((*(uint8_t *)(0xE00FFFD0) & 0x0F ) << 8 ) |
                ((*(uint8_t *)(0xE00FFFE4) & 0xFF ) >> 3 ) |
                ((*(uint8_t *)(0xE00FFFE8) & 0x07 ) << 5 ) + 1 ;
    }
    return  0;
}

void TSys::OnInit()
{
#ifdef STM32F0
    Clock = 48000000;
#elif defined(STM32F1) || defined(GD32F150)
    Clock = 72000000;
#elif defined(STM32F4)
    Clock = 168000000;
#endif
    //CystalClock = 8000000;    // 晶振时钟
    CystalClock = HSE_VALUE;    // 晶振时钟
    MessagePort = COM1; // COM1;

#ifndef TINY
    bool IsGD = Get_JTAG_ID() == 0x7A3;

#if defined(STM32F0) || defined(GD32F150)
	void* p = (void*)0x1FFFF7AC;	// 手册里搜索UID，优先英文手册
#elif defined(STM32F1)
	void* p = (void*)0x1FFFF7E8;
#elif defined(STM32F4)
	void* p = (void*)0x1FFF7A10;
#endif
	Buffer(ID, ArrayLength(ID))	= p;

    CPUID = SCB->CPUID;
    uint mcuid = DBGMCU->IDCODE; // MCU编码。低字设备版本，高字子版本
	if(mcuid == 0 && IsGD) mcuid = *(uint*)0xE0042000; // 用GD32F103的位置
	RevID = mcuid >> 16;
	DevID = mcuid & 0x0FFF;

	// GD32F103 默认使用120M
    if(IsGD && (DevID == 0x0430 || DevID == 0x0414)) Clock = 120000000;

	_Index = 0;
#if defined(STM32F0) || defined(GD32F150)
	if(IsGD)
		FlashSize = *(__IO ushort *)(0x1FFFF7E0);  // 容量
	else
		FlashSize = *(__IO ushort *)(0x1FFFF7CC);  // 容量。手册里搜索FLASH_SIZE，优先英文手册
#elif defined(STM32F1)
    FlashSize = *(__IO ushort *)(0x1FFFF7E0);  // 容量
#elif defined(STM32F4)
    FlashSize = *(__IO ushort *)(0x1FFF7A22);  // 容量
#endif
	if(FlashSize != 0xFFFF)
	{
		RAMSize = FlashSize >> 3;	// 通过Flash大小和MCUID识别型号后得知内存大小
		for(int i=0; i<ArrayLength(MemSizes); i++)
		{
			if(MemSizes[i] == Sys.FlashSize)
			{
				_Index = i;
				break;
			}
		}
		RAMSize = RamSizes[_Index];
	}

	InitHeapStack(StackTop());
#endif

	OnSleep		= nullptr;

#ifdef STM32F1
	// 关闭JTAG仿真接口，只打开SW仿真。
	RCC->APB2ENR	|= RCC_APB2ENR_AFIOEN; // 打开时钟
	AFIO->MAPR		|= AFIO_MAPR_SWJ_CFG_JTAGDISABLE;    //关闭JTAG仿真接口，只打开SW仿真。
#endif

	Code	= 0x0000;
	Ver		= 0x0300;
#ifndef TINY
	Name	= "SmartOS";
	Company	= "NewLife_Embedded_Team";
	//BuildTime = "yyyy-MM-dd HH:mm:ss";

    Interrupt.Init();
#endif

	Started	= false;
}

#pragma arm section code

void ShowTime(void* param)
{
	debug_printf("\r");
	DateTime::Now().Show();
	debug_printf(" ");
}

void TSys::InitClock()
{
#ifndef TINY
    // 获取当前频率
    uint clock = RCC_GetSysClock();
    // 如果当前频率不等于配置，则重新配置时钟
	if(Clock != clock || CystalClock != HSE_VALUE)
	{
		SetSysClock(Clock, CystalClock);

#ifdef STM32F4
		HSE_VALUE = CystalClock;
#endif
		Clock = RCC_GetSysClock();
		SystemCoreClock = Clock;
	}
#endif
}

// 堆起始地址，前面是静态分配内存
uint TSys::HeapBase() const
{
	return (uint)&__heap_base;
}

// 栈顶，后面是初始化不清零区域
uint TSys::StackTop() const
{
	return SRAM_BASE + (RAMSize << 10) - 0x40;
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
	WatchDog::Start();
#endif
}

/******************************** 临界区 ********************************/

void EnterCritical()	{ __disable_irq(); }
void ExitCritical()		{ __enable_irq(); }

/******************************** REV ********************************/

uint	_REV(uint value)		{ return __REV(value); }
ushort	_REV16(ushort value)	{ return __REV16(value); }
