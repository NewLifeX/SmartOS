#include "Sys.h"

#include "Time.h"

TSys Sys;
TTime Time;

extern uint __heap_base;
extern uint __heap_limit;
extern uint __microlib_freelist;
extern uint __microlib_freelist_initialised;

#ifndef BIT
    #define BIT(x)	(1 << (x))
#endif

#ifdef STM32F1
char MemNames[] = "468BCDEFGIK";
uint MemSizes[] = { 16, 32, 64, 128, 256, 384, 512, 768, 1024, 2048, 3072 };
uint RamSizes[] = {  6, 10, 20,  20,  48,  48,  64,  96,   96,   96,   96 };
#elif defined(STM32F0)
char MemNames[] = "468B";
uint MemSizes[] = { 16, 32, 64, 128 };
//uint RamSizes[] = {  4,  6,  8,  16 }; // 150x6有6kRAM
uint RamSizes[] = {  4,  4,  8,  16 };
#elif defined(STM32F4)
char MemNames[] = "468BCDEFGIK";
uint MemSizes[] = { 16, 32, 64, 128, 256, 384, 512, 768, 1024, 2048, 3072 };
uint RamSizes[] = {  6, 10, 20,  20,  48,  48,  64,  96,   96,   96,   96 };
#endif

void TSys::Sleep(uint ms)
{
	// 优先使用线程级睡眠
	if(OnSleep)
		OnSleep(ms);
	else
		Time.Sleep(ms * 1000);
}

void TSys::Delay(uint us)
{
	// 如果延迟微秒数太大，则使用线程级睡眠
	if(OnSleep && us >= 2000)
		OnSleep((us + 500) / 1000);
	else
		Time.Sleep(us);
}

void TSys::Reset() { NVIC_SystemReset(); }

#pragma arm section code

_force_inline void InitHeapStack(uint ramSize)
{
	uint* p = (uint*)__get_MSP();

	// 直接使用RAM最后，需要减去一点，因为TSys构造函数有压栈，待会需要把压栈数据也拷贝过来
	uint top = SRAM_BASE + (ramSize << 10);
	__set_MSP(top - 0x40);	// 左移10位，就是乘以1024
	// 拷贝一部分栈内容到新栈
	memcpy((void*)(top - 0x40), (void*)p, 0x40);

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

bool TSys::CheckMemory()
{
#if DEBUG
	uint msp = __get_MSP();

	//if(__microlib_freelist >= msp) return false;
	assert_param(__microlib_freelist + 0x40 < msp);

	// 如果堆只剩下64字节，则报告失败，要求用户扩大堆空间以免不测
	//uint end = SRAM_BASE + (RAMSize << 10);
	//if(__microlib_freelist + 0x40 >= end) return false;
	assert_param(__microlib_freelist + 0x40 < SRAM_BASE + (RAMSize << 10));
#endif

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

bool SysError(uint code)
{
	debug_printf("系统错误！0x%02x\r\n", code);

#if DEBUG
	ShowFault(code);
#endif

    return true;
}

void SysStop()
{
	debug_printf("系统停止！\r\n");
}

TSys::TSys()
{
    Inited = false;
#if DEBUG
    Debug = true;
#else
    Debug = false;
#endif

#ifdef STM32F0
    Clock = 48000000;
#elif defined(STM32F1)
    Clock = 72000000;
#elif defined(STM32F4)
    Clock = 168000000;
#endif
    //CystalClock = 8000000;    // 晶振时钟
    CystalClock = HSE_VALUE;    // 晶振时钟
    MessagePort = COM1; // COM1;

    IsGD = Get_JTAG_ID() == 0x7A3;

#ifdef STM32F0
	void* p = (void*)0x1FFFF7AC;	// 手册里搜索UID，优先英文手册
#elif defined(STM32F1)
	void* p = (void*)0x1FFFF7E8;
#elif defined(STM32F4)
	void* p = (void*)0x1FFF7A10;
#endif
	memcpy(ID, p, ArrayLength(ID));

    CPUID = SCB->CPUID;
    uint mcuid = DBGMCU->IDCODE; // MCU编码。低字设备版本，高字子版本
	if(mcuid == 0 && IsGD) mcuid = *(uint*)0xE0042000; // 用GD32F103的位置
	RevID = mcuid >> 16;
	DevID = mcuid & 0x0FFF;

	// GD32F103 默认使用120M
    if(IsGD && (DevID == 0x0430 || DevID == 0x0414)) Clock = 120000000;

	_Index = 0;
#ifdef STM32F0
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

	InitHeapStack(RAMSize);

	StartTime = 0;
	OnTick = NULL;
	OnSleep = NULL;

#if DEBUG
    OnError = SysError;
    OnStop = SysStop;
#else
    OnError = 0;
    OnStop = 0;
#endif

#ifdef STM32F10X
	// 关闭JTAG仿真接口，只打开SW仿真。
	RCC->APB2ENR |= RCC_APB2ENR_AFIOEN; // 打开时钟
	AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE;    //关闭JTAG仿真接口，只打开SW仿真。
#endif

    Interrupt.Init();

	_TaskCount = 0;
	//memset(_Tasks, 0, ArrayLength(_Tasks) * sizeof(_Tasks[0]));
	ArrayZero(_Tasks);
	OnStart = NULL;
}

TSys::~TSys()
{
	if(OnStop) OnStop();
}

void ShowTime(void* param)
{
	debug_printf("\r");
	debug_printf(Time.Now().ToString());
	debug_printf(" ");
}

void TSys::Init(void)
{
    // 获取当前频率
    RCC_ClocksTypeDef clock;

    RCC_GetClocksFreq(&clock);
    // 如果当前频率不等于配置，则重新配置时钟
	if(Clock != clock.SYSCLK_Frequency || CystalClock != HSE_VALUE)
	{
		SetSysClock(Clock, CystalClock);

#ifdef STM32F4
		HSE_VALUE = CystalClock;
#endif
		RCC_GetClocksFreq(&clock);
		Clock = clock.SYSCLK_Frequency;
		SystemCoreClock = Clock;
	}

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

	ST_CPUID* cpu = (ST_CPUID*)&CPUID;
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
    debug_printf(" %dMHz Flash:%dk RAM:%dk\r\n", Clock/1000000, FlashSize, RAMSize);
	//debug_printf("\r\n");
    debug_printf("DevID:0x%04X RevID:0x%04X \r\n", DevID, RevID);

    debug_printf("CPUID:0x%08X", CPUID);
	if(cpu->Implementer == 0x41) debug_printf(" ARM");
	if(cpu->Constant == 0x0C)
		debug_printf(" ARMv6-M");
	else if(cpu->Constant == 0x0F)
		debug_printf(" ARMv7-M");
	if((cpu->PartNo & 0x0FF0) == 0x0C20) debug_printf(" Cortex-M%d", cpu->PartNo & 0x0F);
	debug_printf(" R%dp%d", cpu->Revision, cpu->Variant);
    debug_printf("\r\n");
    debug_printf("ChipID:");
	ShowHex(ID, ArrayLength(ID), '-');

	debug_printf("\t");
	ShowString(ID, 12, false);
    debug_printf("\r\n");

	// 输出堆信息
	uint start = (uint)&__heap_base;
	uint end = SRAM_BASE + (RAMSize << 10);
	debug_printf("Heap :(0x%08x, 0x%08x) = 0x%x (%dk)\r\n", start, end, end - start, (end - start) / 1024);
	start = (uint)&__heap_limit;
	//end = 0x20000000 + (RAMSize << 10);
	debug_printf("Stack:(0x%08x, 0x%08x) = 0x%x (%dk)\r\n", start, end, end - start, (end - start) / 1024);

	debug_printf("SystemTime: ");
	debug_printf(Time.Now().ToString());
	debug_printf("\r\n");
	// 系统启动时间
	debug_printf("System Start Cost %dus\r\n", (uint)(Time.Current() - StartTime));

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

uint TSys::Crc(const void* buf, uint len, uint crc)
{
    byte* ptr = (byte*)buf;
    while(len-- > 0)
    {
        crc = c_CRCTable[ ((crc >> 24) ^ (*ptr++)) & 0xFF ] ^ (crc << 8);
    }

    return crc;
}

// 硬件实现的Crc
uint TSys::Crc(const void* buf, uint len)
{
#ifdef STM32F4
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_CRC, ENABLE);
#else
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC, ENABLE);
#endif

    CRC_ResetDR();
    // STM32的初值是0xFFFFFFFF，而软Crc初值是0
	//CRC->DR = __REV(crc ^ 0xFFFFFFFF);
    //CRC->DR = 0xFFFFFFFF;
    uint* ptr = (uint*)buf;
    len >>= 2;
    while(len-- > 0)
    {
        CRC->DR =__REV(*ptr++);    // 字节顺序倒过来,注意不是位序,不是用__RBIT指令
    }
    uint crc = CRC->DR;

#ifdef STM32F4
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_CRC, DISABLE);
#else
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC, DISABLE);
#endif

	return crc;
}

static const ushort c_CRC16Table[] =
{
0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401,
0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400,
};

ushort TSys::Crc16(const void* buf, uint len, ushort crc)
{
    if (!buf || !len) return 0;

    for (int i = 0; i < len; i++)
    {
        byte b = ((byte*)buf)[i];
        crc = (ushort)(c_CRC16Table[(b ^ crc) & 0x0F] ^ (crc >> 4));
        crc = (ushort)(c_CRC16Table[((b >> 4) ^ crc) & 0x0F] ^ (crc >> 4));
    }
    return crc;
}

#endif

#define __HELP__MODULE__ 1
#ifdef __HELP__MODULE__
// 显示十六进制数据，指定分隔字符
void TSys::ShowHex(byte* buf, uint len, char sep)
{
	for(int i=0; i < len; i++)
	{
		debug_printf("%02X", *buf++);
		if(i < len - 1 && sep != '\0') debug_printf("%c", sep);
		//if(((i + 1) & 0xF) == 0) debug_printf("\r\n");
	}
	//debug_printf("\r\n");
}

// 显示字符串，不指定长度时自动找\0
void TSys::ShowString(byte* buf, uint len, bool autoEnd)
{
	if(len == 0) len = 1000;
    for(int i=0; i<len; i++)
    {
		if(buf[i] == 0 && autoEnd) return;
		if(buf[i] >= 32 && buf[i] <= 126 || buf[i] == 0x0A || buf[i] == 0x0D || buf[i] == 0x09)
			debug_printf("%c", buf[i]);
		else
			debug_printf("%02X", buf[i]);
    }
}

// 源数据转为十六进制字符编码再放入目标字符，比如0x28在目标放两个字节0x02 0x08
void TSys::ToHex(byte* buf, byte* src, uint len)
{
	for(int i=0; i < len; i++, src++)
	{
		byte n = *src >> 4;
		if(n < 10)
			n += '0';
		else
			n += 'A' - 10;
		*buf++ = n;

		n = *src & 0x0F;
		if(n < 10)
			n += '0';
		else
			n += 'A' - 10;
		*buf++ = n;
	}
}
#endif

#define __TASK__MODULE__ 1
#ifdef __TASK__MODULE__
// 创建任务，返回任务编号。priority优先级，dueTime首次调度时间us，period调度间隔us，-1表示仅处理一次
uint TSys::AddTask(Action func, void* param, ulong dueTime, long period)
{
	// 屏蔽中断，否则可能有线程冲突
	SmartIRQ irq;

	// 找一个空闲位给它
	int i = 0;
	while(i < ArrayLength(_Tasks) && _Tasks[i] != NULL) i++;
	assert_param(i < ArrayLength(_Tasks));

	Task* task = new Task();
	_Tasks[i] = task;
	task->ID = i + 1;
	task->Callback = func;
	task->Param = param;
	task->Period = period;
	task->NextTime = Time.Current() + dueTime;

	_TaskCount++;
	// 输出长整型%ld，无符号长整型%llu
	debug_printf("添加任务%d 0x%08x FirstTime=%lluus Period=%ldus\r\n", task->ID, func, dueTime, period);

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
	if(_Running) return;

#if DEBUG
	//AddTask(ShowTime, NULL, 2000000, 2000000);
#endif
	debug_printf("系统准备就绪，开始循环处理%d个任务！\r\n", _TaskCount);

	if(OnStart)
		OnStart();
	else
		StartInternal();
}

void TSys::StartInternal()
{
	_Running = true;
	while(_Running)
	{
		ulong now = Time.Current();	// 当前时间
		ulong min = UInt64_Max;	// 最小时间，这个时间就会有任务到来
		int k = 0;
		for(int i=0; i < ArrayLength(_Tasks) && k < _TaskCount; i++)
		{
			Task* task = _Tasks[i];
			if(task)
			{
				if(task->NextTime <= now)
				{
					// 先计算下一次时间
					//task->NextTime += task->Period;
					// 不能通过累加的方式计算下一次时间，因为可能系统时间被调整
					task->NextTime = now + task->Period;
					if(task->NextTime < min) min = task->NextTime;

					task->Callback(task->Param);

					// 如果只是一次性任务，在这里清理
					if(task->Period < 0)
					{
						_Tasks[i] = NULL;
						delete task;
						_TaskCount--;
					}
				}

				k++;
			}
		}

		// 如果有最小时间，睡一会吧
		now = Time.Current();	// 当前时间
		if(min != UInt64_Max && min > now) Delay(min - now);
	}
	debug_printf("系统停止调度，共有%d个任务！\r\n", _TaskCount);
}

void TSys::Stop()
{
	debug_printf("系统停止！\r\n");
	_Running = false;
}
#endif

const char* Object::ToString()
{
	const char* str = typeid(*this).name();
	while(*str >= '0' && *str <= '9') str++;

	return str;
}
