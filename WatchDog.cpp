#include "WatchDog.h"

/*
独立看门狗IWDG——有独立时钟（内部低速时钟LSI---40KHz），所以不受系统硬件影响的系统故障探测器。主要用于监视硬件错误。 
窗口看门狗WWDG——时钟与系统相同。如果系统时钟不走了，这个狗也就失去作用了，主要用于监视软件错误。 

独立看门狗
看门狗定时时限=IWDG_SetReload()的值 / 看门狗时钟频率
看门狗时钟频率=LSI（内部低速时钟）的频率（40KHz）/ 分频数

独立看门狗(IWDG)由专用的40kHz的低速时钟为驱动。因此，即使主时钟发生故障它也仍然有效。
窗口看门狗由从APB1时钟分频后得到的时钟驱动，通过可配置的时间窗口来检测应用程序非正常的过迟或过早的行为。
IWDG_SetPrescaler()		分频，4~256
IWDG_SetReload()		计数值12位 0~0x0FFF
IWDG_ReloadCounter()	喂狗，必须在计数值减到1之前喂狗

IWDG_PR和IWDG_RLR寄存器具有写保护功能。要修改这两个寄存器的值，必须先向IWDG_KR寄存器中写入0x5555。
以不同的值写入这个寄存器将会打乱操作顺序，寄存器将重新被保护。重装载操作(即写入0xAAAA)也会启动写保护功能。

#define IWDG_WriteAccess_Enable     ((uint16_t)0x5555)
#define IWDG_WriteAccess_Disable    ((uint16_t)0x0000)

void IWDG_WriteAccessCmd(uint16_t IWDG_WriteAccess)
{
  assert_param(IS_IWDG_WRITE_ACCESS(IWDG_WriteAccess));
  IWDG->KR = IWDG_WriteAccess;
}

void IWDG_ReloadCounter(void)
{
  IWDG->KR = KR_KEY_Reload;
}
void IWDG_Enable(void)
{
  IWDG->KR = KR_KEY_Enable;
}

void IWDG_SetPrescaler(uint8_t IWDG_Prescaler)
{
  assert_param(IS_IWDG_PRESCALER(IWDG_Prescaler));
  IWDG->PR = IWDG_Prescaler;
}
void IWDG_SetReload(uint16_t Reload)
{
  assert_param(IS_IWDG_RELOAD(Reload));
  IWDG->RLR = Reload;
}

*/

WatchDog::WatchDog(uint ms)
{
	Timeout = ms;
	Config(ms);
}

WatchDog::~WatchDog()
{
	ConfigMax();
}

bool WatchDog::Config(uint ms)
{
	if(ms == 0)
	{
		debug_printf("WatchDog msTimeout %dms must larger than 0ms\r\n", ms);
		return false;
	}
	RCC_LSICmd(ENABLE);
	/* 检查系统是否从IWDG重置恢复 */
	if (RCC_GetFlagStatus(RCC_FLAG_IWDGRST) != RESET)
	{
		/* 清除重置标识 */
		RCC_ClearFlag();
	}
	/* 打开IWDG_PR和IWDG_RLR寄存器的写访问 */
	IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);

	byte pre = IWDG_Prescaler_4;
	int mul = 4;
	// 计数器12位 0~0x0FFF，有reload = ms/1000 / (1/(LSI/mul)) = ms * LSI / (mul*1000) = ms * 40 / mul
	// 考虑到reload溢出的可能，每种分频最大ms = reload * mul / 40 ~= 102 * mul
	int i=0;
	/*
	#define IWDG_Prescaler_4            ((uint8_t)0x00)
	#define IWDG_Prescaler_8            ((uint8_t)0x01)
	#define IWDG_Prescaler_16           ((uint8_t)0x02)
	#define IWDG_Prescaler_32           ((uint8_t)0x03)
	#define IWDG_Prescaler_64           ((uint8_t)0x04)
	#define IWDG_Prescaler_128          ((uint8_t)0x05)
	#define IWDG_Prescaler_256          ((uint8_t)0x06)
	*/
	for(i = IWDG_Prescaler_4; i <= IWDG_Prescaler_256; i++)
	{
		pre = i;
		mul = 1 << (i + 2);

		// 判断是否在范围之内
		if(ms * 40 / mul < 0x0FFF) break;
	}
	if(i > IWDG_Prescaler_256)
	{
		debug_printf("WatchDog msTimeout must smaller than %dms\r\n", 0x0FFF * 256 / 40);
		return false;
	}
	IWDG_SetPrescaler(pre);
	/*if(ms < (0x0FFF * 32 / 40))
	{
		// IWDG计数器时钟: LSI/32=40KHz/32=1250Hz，每周期0.8ms
		IWDG_SetPrescaler(IWDG_Prescaler_32);
	}
	else
	{
		// IWDG计数器时钟: LSI/64=40KHz/64=625Hz，每周期0.4ms
		IWDG_SetPrescaler(IWDG_Prescaler_64);

		// 直接除以2，后面不用重复计算
		ms >>= 2;
	}*/

	/* 设置计数器重载值为超时时间
	 Counter Reload Value = ms / 1000 / IWDG计数器时钟周期
						  = ms / 1000 / (1/(LSI/mul))
						  = ms * LSI / (mul * 1000)
						  = ms * 40k / (mul * 1000)
						  = ms * 40 / mul
	*/
	IWDG_SetReload(ms * 40 / mul);

	/* 重载 IWDG 计数器 */
	IWDG_ReloadCounter();

	/* 打开 IWDG (LSI将由硬件打开) */
	IWDG_Enable();

	Timeout = ms;

	return true;
}

void WatchDog::ConfigMax()
{
	/* 打开IWDG_PR和IWDG_RLR寄存器的写访问 */
	IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);

	// 独立看门狗无法关闭
	IWDG_SetPrescaler(IWDG_Prescaler_256);
	IWDG_SetReload(0x0FFF);
	IWDG_ReloadCounter();
}

void WatchDog::Feed()
{
	IWDG_ReloadCounter();
}
