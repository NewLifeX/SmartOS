#include "Sys.h"

#include "Time.h"
#include <stdlib.h>

TSys Sys;
TTime Time;

#ifndef BIT
    #define BIT(x)	(1 << (x))
#endif

#define  RCC_CFGR_USBPRE_Div1                     ((uint32_t)0x00400000)        /*!< USB Device prescaler */
#define  RCC_CFGR_USBPRE_1Div5                    ((uint32_t)0x00000000)        /*!< USB Device prescaler */
#define  RCC_CFGR_USBPRE_Div2                     ((uint32_t)0x00C00000)        /*!< USB Device prescaler */
#define  RCC_CFGR_USBPRE_2Div5                    ((uint32_t)0x00800000)        /*!< USB Device prescaler */

#define GD32_PLL_MASK	0x20000000
#define CFGR_PLLMull_Mask         ((uint32_t)0x003C0000)

#ifdef STM32F10X
char MemNames[] = "468BCDEFGIK";
uint MemSizes[] = { 16, 32, 64, 128, 256, 384, 512, 768, 1024, 2048, 3072 };
uint RamSizes[] = {  6, 10, 10,  20,  48,  48,  64,  96,   96,   96,   96 };
#elif defined(STM32F0XX)
char MemNames[] = "468B";
uint MemSizes[] = { 16, 32, 64, 128 };
//uint RamSizes[] = {  4,  6,  8,  16 }; // 150x6有6kRAM
uint RamSizes[] = {  4,  4,  8,  16 };
#endif

void TSys::Sleep(uint ms) { Time.Sleep(ms * 1000); }

void TSys::Delay(uint us) { Time.Sleep(us); }

void TSys::Reset() { NVIC_SystemReset(); }

// 原配置MSP作为PSP，而使用全局数组作为新的MSP
// MSP 堆栈大小
#define IRQ_STACK_SIZE 0x100
uint IRQ_STACK[IRQ_STACK_SIZE]; // MSP 中断嵌套堆栈

#pragma arm section code

force_inline void Set_SP(uint ramSize)
{
	uint p = __get_MSP();
	if(!ramSize)
		__set_PSP(p);
	else
		// 直接使用RAM最后，需要减去一点，因为TSys构造函数有压栈，待会需要把压栈数据也拷贝过来
		__set_PSP(0x20000000 + (ramSize << 10) - 0x40);	// 左移10位，就是乘以1024
	__set_MSP((uint)&IRQ_STACK[IRQ_STACK_SIZE]);
	__set_CONTROL(2);
	__ISB();
	
	// 拷贝一部分栈内容到新栈
	memcpy((void*)__get_PSP(), (void*)p, 0x40);
}

bool TSys::CheckMemory()
{
	uint msp = __get_MSP();
	if(msp < (uint)&IRQ_STACK[0] || msp > (uint)&IRQ_STACK[IRQ_STACK_SIZE]) return false;

	uint psp = __get_PSP();
	void* p = malloc(0x10);
	if(!p) return false;
	free(p);
	if((uint)p >= psp) return false;

	return true;
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

#if defined(GD32) && defined(STM32F10X)
void Bootstrap()
{
    uint n = 0;
    uint RCC_CFGR_ADC_BITS = RCC_CFGR_ADCPRE_DIV8;
    uint FLASH_ACR_LATENCY_BITS = 0;
    uint RCC_CFGR_USBPRE_BIT = 0;
    uint mull, mull2;
    // 是否GD
    bool isGD = Get_JTAG_ID() == 0x7A3;
    if(isGD && Sys.Clock == 72000000) Sys.Clock = 120000000;
    // 确保关闭中断，为了方便调试，Debug时不关闭
#ifndef DEBUG
    //__disable_irq();
#endif
    n = Sys.Clock / 14000000;
    if (n < 6)
        RCC_CFGR_ADC_BITS = RCC_CFGR_ADCPRE_DIV6;
    else
        RCC_CFGR_ADC_BITS = RCC_CFGR_ADCPRE_DIV8;
    // GD32的Flash零等待
    if (isGD)
    {
        // 对于GD来说，这个寄存器位其实没有什么意义，内部设定好等待时间了
        // GD 256k以内是零等待，512k以内是1等待，以后是2等待
        FLASH_ACR_LATENCY_BITS = FLASH_ACR_LATENCY_0; // 零等待
    }
    else
    {
        //FLASH_ACR_LATENCY_BITS = FLASH_ACR_LATENCY_2; // 等待两个
        n = Sys.Clock / 24000000 - 1;
        FLASH_ACR_LATENCY_BITS = FLASH_ACR_LATENCY_0 + n;
    }
    
    // 配置JTAG调试支持
    DBGMCU->CR = DBGMCU_CR_DBG_TIM2_STOP | DBGMCU_CR_DBG_SLEEP;

    // 允许访问未对齐内存，不强制8字节栈对齐
    SCB->CCR &= ~(SCB_CCR_UNALIGN_TRP | SCB_CCR_STKALIGN);

    // 捕获所有异常
    CoreDebug->DEMCR |= CoreDebug_DEMCR_VC_HARDERR_Msk | CoreDebug_DEMCR_VC_INTERR_Msk |
                        CoreDebug_DEMCR_VC_BUSERR_Msk | CoreDebug_DEMCR_VC_STATERR_Msk |
                        CoreDebug_DEMCR_VC_CHKERR_Msk | CoreDebug_DEMCR_VC_NOCPERR_Msk |
                        CoreDebug_DEMCR_VC_MMERR_Msk;

    // 为了配置时钟，CPU必须运行在8MHz晶振上
    RCC->CR |= RCC_CR_HSION;
    while(!(RCC->CR & RCC_CR_HSIRDY));

	// 切换到内部时钟
    RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_SW));
	// RCC_CFGR_SW_HSI是0，一点意义都没有
    RCC->CFGR |= RCC_CFGR_SW_HSI;      // sysclk = AHB = APB1 = APB2 = HSI (8MHz)

    // 关闭 HSE 时钟
    RCC->CR &= ~RCC_CR_HSEON;
    // 关闭 HSE & PLL
    RCC->CR &= ~RCC_CR_PLLON;

	RCC->CR |= ((uint32_t)RCC_CR_HSEON);
	while(!(RCC->CR & RCC_CR_HSERDY));

    // 设置Flash访问时间，并打开预读缓冲
    FLASH->ACR = FLASH_ACR_LATENCY_BITS | FLASH_ACR_PRFTBE;

    RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL));
	// 支持超频，主频必须是8M的倍频
	mull = Sys.Clock / Sys.CystalClock;
	mull2 = (mull - 2) << 18;
	// 处理0.5倍频
	if( (mull * Sys.CystalClock + Sys.CystalClock / 2) == Sys.Clock )
	{
		mull = 2 * Sys.Clock / Sys.CystalClock;
		// 对于GD32的108MHz，没有除尽。
		if( isGD && mull > 17 )
			mull2 = (mull - 17) << 18 | RCC_CFGR_PLLXTPRE_HSE_Div2 | GD32_PLL_MASK;
		else
			mull2 = (mull - 2 ) << 18 | RCC_CFGR_PLLXTPRE_HSE_Div2;
	}

	RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLSRC_HSE | mull2);

    switch ( 2 * Sys.Clock / 48000000 )
    {
        case 2:
            RCC_CFGR_USBPRE_BIT = RCC_CFGR_USBPRE_Div1;
			break;
        case 3:
            RCC_CFGR_USBPRE_BIT = RCC_CFGR_USBPRE_1Div5;
			break;
        case 4:
            RCC_CFGR_USBPRE_BIT = RCC_CFGR_USBPRE_Div2;
			break;
        case 5:
            RCC_CFGR_USBPRE_BIT = RCC_CFGR_USBPRE_2Div5;
			break;
        default:
            RCC_CFGR_USBPRE_BIT = RCC_CFGR_USBPRE_Div1;
			break;
    }

    // 最终时钟配置
    RCC->CFGR |= RCC_CFGR_USBPRE_BIT     // USB clock
              | RCC_CFGR_HPRE_DIV1  // AHB clock
              | RCC_CFGR_PPRE1_DIV2 // APB1 clock
              | RCC_CFGR_PPRE2_DIV1 // APB2 clock
              | RCC_CFGR_ADC_BITS;      // ADC clock (max 14MHz)
    
    // 打开 HSE & PLL
    RCC->CR |= RCC_CR_PLLON;// | RCC_CR_HSEON;

    // 等到 PPL 达到时钟频率
    while(!(RCC->CR & RCC_CR_PLLRDY));

    // 最终时钟配置
    RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_SW));
    RCC->CFGR |= RCC_CFGR_SW_PLL;         // sysclk = pll out (SYSTEM_CLOCK_HZ)
	while ((RCC->CFGR & (uint32_t)RCC_CFGR_SWS) != (uint32_t)0x08);

    // 最小化外设时钟
    RCC->AHBENR  = RCC_AHBENR_SRAMEN | RCC_AHBENR_FLITFEN;
    RCC->APB2ENR = RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR = RCC_APB1ENR_PWREN;

    // 关闭 HSI 时钟
    RCC->CR &= ~RCC_CR_HSION;

	// 计算当前工作频率
	mull = RCC->CFGR & (CFGR_PLLMull_Mask | GD32_PLL_MASK);
	if((mull & GD32_PLL_MASK) != 0) // 兼容GD32的108MHz
		mull = ((mull & CFGR_PLLMull_Mask) >> 18) + 17;
	else
		mull = ( mull >> 18) + 2;
	//TSys::Frequency = HSI_VALUE * pllmull;
	// 不能直接用HSI_VALUE，因为不同的硬件设备使用不同的晶振
	Sys.Clock = Sys.CystalClock * mull;
	if( (RCC->CFGR & RCC_CFGR_PLLXTPRE_HSE_Div2) && !isGD ) Sys.Clock /= 2;
}
#endif

void ShowError(int code) { debug_printf("系统错误！%d\r\n", code); }

TSys::TSys()
{
#if DEBUG
    Debug = true;
#else
    Debug = false;
#endif
    Inited = false;

    Clock = 72000000;
    CystalClock = 8000000;    // 晶振时钟
    MessagePort = 0; // COM1;

    IsGD = Get_JTAG_ID() == 0x7A3;
    if(IsGD) Clock = 120000000;

	if(IsGD)
	{
		void* p = (void*)0x1FFFF7AC;
		memcpy(ID, p, 12);
	}
	else
	{
		void* p = (void*)0x1FFFF7E8;
#ifdef STM32F0XX
		memcpy(ID, p, 4);
#else
		memcpy(ID, p, 12);
#endif
	}

    CPUID = SCB->CPUID;
    uint MCUID = DBGMCU->IDCODE; // MCU编码。低字设备版本，高字子版本
	RevID = MCUID >> 16;
	DevID = MCUID & 0x0FFF;

    FlashSize = *(__IO ushort *)(0x1FFFF7E0);  // 容量
	if(FlashSize == 0xFFFF)
		FlashSize = RAMSize = 0;
	else
	{
		RAMSize = FlashSize >> 3;	// 通过Flash大小和MCUID识别型号后得知内存大小
		for(int i=0; i<ArrayLength(MemSizes); i++)
		{
			if(MemSizes[i] == Sys.FlashSize)
			{
				RAMSize = RamSizes[i];
				break;
			}
		}
	}

	Set_SP(RAMSize);

#if DEBUG
    OnError = ShowError;
#else
    OnError = 0;
#endif

#ifdef STM32F10X
	// 关闭JTAG仿真接口，只打开SW仿真。
	RCC->APB2ENR |= RCC_APB2ENR_AFIOEN; // 打开时钟
	AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE;    //关闭JTAG仿真接口，只打开SW仿真。
#endif

    Interrupt.Init();
	
	_TaskCount = 0;
	memset(_Tasks, 0, ArrayLength(_Tasks));
}

TSys::~TSys()
{
    //if(g_Time) delete g_Time;
    //g_Time = NULL;
}

void TSys::Init(void)
{
    // 获取当前频率
    RCC_ClocksTypeDef clock;

    RCC_GetClocksFreq(&clock);
#if defined(GD32) && defined(STM32F10X)
    // 如果当前频率不等于配置，则重新配置时钟
	if(Clock != clock.SYSCLK_Frequency) Bootstrap();
#else
    Clock = clock.SYSCLK_Frequency;
#endif

#ifdef STM32F10X
	NVIC_PriorityGroupConfig( NVIC_PriorityGroup_4);	//中断优先级分配方案4   四位都是抢占优先级
#endif

	// 必须在系统主频率确定以后再初始化时钟
    Time.Init();

    Inited = true;
}

#if DEBUG
typedef struct
{
	byte Revision:4;	// The p value in the Rnpn product revision identifier, indicates patch release.0x0: patch 0
	ushort PartNo:12;		// Part number of the processor. 0xC20: Cortex-M0
	byte Constant:4;	// Constant that defines the architecture of the processor. 0xC: ARMv6-M architecture
	byte Variant:4;		// Variant number: The r value in the Rnpn product revision identifier. 0x0: revision 0
	byte Implementer;	// Implementer code. 0x41 ARM
}ST_CPUID;
#endif

void TSys::ShowInfo()
{
#if DEBUG
	debug_printf("SmartOS ");
	if(IsGD)
		debug_printf("GD32");
	else
		debug_printf("STM32");
	if(DevID == 0x414 || DevID == 0x430)
		debug_printf("F103");
	// 暂时不知道怎么计算引脚
	debug_printf("V");
	
	for(int i=0; i<ArrayLength(MemSizes); i++)
	{
		if(MemSizes[i] == FlashSize)
		{
			debug_printf("%c", MemNames[i]);
			break;
		}
	}
	//debug_printf("\r\n");

    // 系统信息
    debug_printf(" %dMHz Flash:%dk RAM:%dk\r\n", Clock/1000000, FlashSize, RAMSize);
    debug_printf("芯片：%02X", ID[0]);
	for(int i=1; i<12; i++) debug_printf("-%02X", ID[i]);
	debug_printf("\r\n");
    debug_printf("DevID:0x%04X RevID:0x%04X \r\n", DevID, RevID);

    debug_printf("CPUID:0x%08X", CPUID);
	//uint cpuid = __REV(CPUID);
	ST_CPUID* cpu = (ST_CPUID*)&CPUID;
	if(cpu->Implementer == 0x41) debug_printf(" ARM");
	if(cpu->Constant == 0x0C) debug_printf(" ARMv6-M architecture");
	if(cpu->PartNo == 0x0C20) debug_printf(" Cortex-M0");
	//if(cpu->PartNo2 == 0x20) debug_printf("M0");
	debug_printf(" R%dp%d", cpu->Revision, cpu->Variant);
    debug_printf("\r\n");
	
    debug_printf("\r\n");
#endif
}

#define __CRC__MODULE__ 1
#ifdef __CRC__MODULE__
//
// CRC 32 table for use under ZModem protocol, IEEE 802
// G(x) = x^32+x^26+x^23+x^22+x^16+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x+1
//
static const uint c_CRCTable[ 256 ] =
{
    0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9, 0x130476DC, 0x17C56B6B, 0x1A864DB2, 0x1E475005,
    0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6, 0x2B4BCB61, 0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD,
    0x4C11DB70, 0x48D0C6C7, 0x4593E01E, 0x4152FDA9, 0x5F15ADAC, 0x5BD4B01B, 0x569796C2, 0x52568B75,
    0x6A1936C8, 0x6ED82B7F, 0x639B0DA6, 0x675A1011, 0x791D4014, 0x7DDC5DA3, 0x709F7B7A, 0x745E66CD,
    0x9823B6E0, 0x9CE2AB57, 0x91A18D8E, 0x95609039, 0x8B27C03C, 0x8FE6DD8B, 0x82A5FB52, 0x8664E6E5,
    0xBE2B5B58, 0xBAEA46EF, 0xB7A96036, 0xB3687D81, 0xAD2F2D84, 0xA9EE3033, 0xA4AD16EA, 0xA06C0B5D,
    0xD4326D90, 0xD0F37027, 0xDDB056FE, 0xD9714B49, 0xC7361B4C, 0xC3F706FB, 0xCEB42022, 0xCA753D95,
    0xF23A8028, 0xF6FB9D9F, 0xFBB8BB46, 0xFF79A6F1, 0xE13EF6F4, 0xE5FFEB43, 0xE8BCCD9A, 0xEC7DD02D,
    0x34867077, 0x30476DC0, 0x3D044B19, 0x39C556AE, 0x278206AB, 0x23431B1C, 0x2E003DC5, 0x2AC12072,
    0x128E9DCF, 0x164F8078, 0x1B0CA6A1, 0x1FCDBB16, 0x018AEB13, 0x054BF6A4, 0x0808D07D, 0x0CC9CDCA,
    0x7897AB07, 0x7C56B6B0, 0x71159069, 0x75D48DDE, 0x6B93DDDB, 0x6F52C06C, 0x6211E6B5, 0x66D0FB02,
    0x5E9F46BF, 0x5A5E5B08, 0x571D7DD1, 0x53DC6066, 0x4D9B3063, 0x495A2DD4, 0x44190B0D, 0x40D816BA,
    0xACA5C697, 0xA864DB20, 0xA527FDF9, 0xA1E6E04E, 0xBFA1B04B, 0xBB60ADFC, 0xB6238B25, 0xB2E29692,
    0x8AAD2B2F, 0x8E6C3698, 0x832F1041, 0x87EE0DF6, 0x99A95DF3, 0x9D684044, 0x902B669D, 0x94EA7B2A,
    0xE0B41DE7, 0xE4750050, 0xE9362689, 0xEDF73B3E, 0xF3B06B3B, 0xF771768C, 0xFA325055, 0xFEF34DE2,
    0xC6BCF05F, 0xC27DEDE8, 0xCF3ECB31, 0xCBFFD686, 0xD5B88683, 0xD1799B34, 0xDC3ABDED, 0xD8FBA05A,
    0x690CE0EE, 0x6DCDFD59, 0x608EDB80, 0x644FC637, 0x7A089632, 0x7EC98B85, 0x738AAD5C, 0x774BB0EB,
    0x4F040D56, 0x4BC510E1, 0x46863638, 0x42472B8F, 0x5C007B8A, 0x58C1663D, 0x558240E4, 0x51435D53,
    0x251D3B9E, 0x21DC2629, 0x2C9F00F0, 0x285E1D47, 0x36194D42, 0x32D850F5, 0x3F9B762C, 0x3B5A6B9B,
    0x0315D626, 0x07D4CB91, 0x0A97ED48, 0x0E56F0FF, 0x1011A0FA, 0x14D0BD4D, 0x19939B94, 0x1D528623,
    0xF12F560E, 0xF5EE4BB9, 0xF8AD6D60, 0xFC6C70D7, 0xE22B20D2, 0xE6EA3D65, 0xEBA91BBC, 0xEF68060B,
    0xD727BBB6, 0xD3E6A601, 0xDEA580D8, 0xDA649D6F, 0xC423CD6A, 0xC0E2D0DD, 0xCDA1F604, 0xC960EBB3,
    0xBD3E8D7E, 0xB9FF90C9, 0xB4BCB610, 0xB07DABA7, 0xAE3AFBA2, 0xAAFBE615, 0xA7B8C0CC, 0xA379DD7B,
    0x9B3660C6, 0x9FF77D71, 0x92B45BA8, 0x9675461F, 0x8832161A, 0x8CF30BAD, 0x81B02D74, 0x857130C3,
    0x5D8A9099, 0x594B8D2E, 0x5408ABF7, 0x50C9B640, 0x4E8EE645, 0x4A4FFBF2, 0x470CDD2B, 0x43CDC09C,
    0x7B827D21, 0x7F436096, 0x7200464F, 0x76C15BF8, 0x68860BFD, 0x6C47164A, 0x61043093, 0x65C52D24,
    0x119B4BE9, 0x155A565E, 0x18197087, 0x1CD86D30, 0x029F3D35, 0x065E2082, 0x0B1D065B, 0x0FDC1BEC,
    0x3793A651, 0x3352BBE6, 0x3E119D3F, 0x3AD08088, 0x2497D08D, 0x2056CD3A, 0x2D15EBE3, 0x29D4F654,
    0xC5A92679, 0xC1683BCE, 0xCC2B1D17, 0xC8EA00A0, 0xD6AD50A5, 0xD26C4D12, 0xDF2F6BCB, 0xDBEE767C,
    0xE3A1CBC1, 0xE760D676, 0xEA23F0AF, 0xEEE2ED18, 0xF0A5BD1D, 0xF464A0AA, 0xF9278673, 0xFDE69BC4,
    0x89B8FD09, 0x8D79E0BE, 0x803AC667, 0x84FBDBD0, 0x9ABC8BD5, 0x9E7D9662, 0x933EB0BB, 0x97FFAD0C,
    0xAFB010B1, 0xAB710D06, 0xA6322BDF, 0xA2F33668, 0xBCB4666D, 0xB8757BDA, 0xB5365D03, 0xB1F740B4
};

uint TSys::Crc(const void* rgBlock, int len, uint crc)
{
    const byte* ptr = (const byte*)rgBlock;

    while(len-- > 0)
    {
        crc = c_CRCTable[ ((crc >> 24) ^ (*ptr++)) & 0xFF ] ^ (crc << 8);
    }

    return crc;
}
#endif

#define __TASK__MODULE__ 1
#ifdef __TASK__MODULE__
// 创建任务，返回任务编号。priority优先级，dueTime首次调度时间us，period调度间隔us，-1表示仅处理一次
uint TSys::AddTask(Action func, void* param, uint dueTime, int period)
{
	// 屏蔽中断，否则可能有线程冲突
	SmartIRQ irq;

	// 找一个空闲位给它
	int i = 0;
	while(i < ArrayLength(_Tasks) && _Tasks[i++] != NULL);
	assert_param(i < ArrayLength(_Tasks));

	Task* task = new Task();
	_Tasks[i - 1] = task;
	task->ID = i;
	task->Callback = func;
	task->Param = param;
	task->Period = period;
	task->NextTime = Time.Current() + dueTime;
	
	_TaskCount++;
	debug_printf("添加任务%d 0x%08x\r\n", task->ID, func);
	
	return task->ID;
}

void TSys::RemoveTask(uint taskid)
{
	assert_param(taskid > 0);
	assert_param(taskid <= _TaskCount);
	
	Task* task = _Tasks[taskid - 1];
	_Tasks[taskid - 1] = NULL;
	if(task)
	{
		debug_printf("删除任务%d 0x%08x\r\n", task->ID, task->Callback);
		delete task;
		
		_TaskCount--;
	}
}

void TSys::Start()
{
	debug_printf("系统准备就绪，开始循环处理%d个任务！\r\n", _TaskCount);

	_Running = true;
	while(_Running)
	{
		ulong now = Time.Current();	// 当前时间
		int k = 0;
		for(int i=0; i < ArrayLength(_Tasks) && k < _TaskCount; i++)
		{
			Task* task = _Tasks[i];
			if(task && task->NextTime <= now)
			{
				// 先计算下一次时间
				task->NextTime += task->Period;
				task->Callback(task->Param);
				
				// 如果只是一次性任务，在这里清理
				if(task->Period < 0)
				{
					_Tasks[i] = NULL;
					delete task;
					_TaskCount--;
				}
				
				k++;
			}
		}
	}
	debug_printf("系统停止调度，共有%d个任务！\r\n", _TaskCount);
}

void TSys::Stop()
{
	debug_printf("系统停止！\r\n");
	_Running = false;
	
	// 销毁所有任务
	/*for(int i=0; i < ArrayLength(_Tasks); i++)
	{
		Task* task = _Tasks[i];
		if(task) delete task;
	}
	memset(_Tasks, 0, ArrayLength(_Tasks));*/
}
#endif
