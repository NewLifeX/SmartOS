#include "IR.h"
#include "Time.h"

#ifdef STM32F0
#include "stm32f0xx_tim.h"
#endif
/*
Timer2  CH2       通道
DMA1    Channel3  通道

*/

IR::IR(PWM * pwm)
{
	_Pwm = pwm;
}

bool IR::Open()
{
	return true;
}

bool IR::Close()
{
	return true;
}

typedef enum
{
	WaitRev,
	Rev,
	RevOver,
	
	Sending,
	SendOver,
}IRStat;
IRStat Stat;

ushort * SendP = NULL;
ushort SendIndex;
ushort SendBufLen = 0;
bool test;

bool IR::Send(const Array& bs)
{
#ifdef STM32F0
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	if(_Tim == NULL)
	{
		// RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
		_Tim = Timer::Create(0x01);		// 直接占用TIMER2
	}
	// _Tim->SetFrequency(50);		// 5Hz   20ms
	// 使用一个字节  需要自行配置分频系数
	_Tim->Prescaler	= 400;		// 分频 需要结果是 Period < 256 时能计数 20ms
	
	SendP = (ushort *)bs.GetBuffer();
	SendBufLen	= bs.Length()/2;
	
	SendIndex = 0;
	_Tim->Period	= SendP[0];
	// 取消接收时候的配置
	_Tim->Opened = true;
	_Tim->Close();
	
	// 注册中断
	_Tim->Register(OnSend,this);
	// 重新配置
	_Tim->SetCounter(0x00000000);
	_Tim->Config();
	
	Stat = Sending;
	debug_printf("Start %04X\r\n",SendP[0]);
		
	// 开始  中断内处理数据
	// _Pwm->Open();
	test = true;
	_Tim->Open();
	// 等待结束
	TimeWheel tw(2);
	while(!tw.Expired() && Stat != SendOver);
	// 结束 不论是超时还是发送结束都关闭pwm和定时器
	//_Pwm->Close();
	_Tim->Close();
	// 清空使用的数据
	SendIndex = 0;
	SendP = NULL;
#endif
	return true;
}

void IR::OnSend(void* sender, void* param)
{
	TS("IR::OnSend");
#ifdef STM32F0
	auto ir = (IR*)param;
	if(test)	// 避开打开定时器立马中断问题
	{
		ir->_Pwm->Open();
		test = false;
		return;
	}
	// PWM归位
	TIM_SetCounter(ir->_Pwm->_Timer, 0x00000000);
	// PWM变化
	if(SendIndex % 2)
		TIM_Cmd(ir->_Pwm->_Timer, DISABLE);
		//ir->_Pwm->Close();
	else
		TIM_Cmd(ir->_Pwm->_Timer, ENABLE);
		//ir->_Pwm->Open();
	// 下一个周期
	SendIndex++;
	// 不能使用这个  会复位定时器计数 严重拖延时间
	//ir->_Tim->Period = SendP[SendIndex];
	//ir->_Tim->Config();
	TIM_SetAutoreload(ir->_Tim->_Timer, SendP[SendIndex]);

	if(SendIndex >= SendBufLen)
	{
		// 发送完毕
		TIM_SetCounter(ir->_Pwm->_Timer, 0x00000000);
		TIM_Cmd(ir->_Pwm->_Timer, DISABLE);
		//ir->_Pwm->Close();
		ir->_Tim->Close();
		Stat = SendOver;
		debug_printf("SendOver SendIndex %d\r\n",SendIndex);
		return;
	}
#endif
}

int IR::Receive(Array& bs, int sTimeout)
{
	TS("IR::Receive");
#ifdef STM32F0
	uint bufLen	= bs.Length();
	uint DmaLen	= bufLen/2;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, DISABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	
	if(_Tim == NULL)
	{
		_Tim = Timer::Create(0x01);		// 直接占用TIMER2
		// _Tim->SetFrequency(50);		// 5Hz   20ms
		// 使用一个字节  需要自行配置分频系数
		_Tim->Prescaler	= 400;		// 分频 需要结果是 Period < 256 时能计数 20ms
		_Tim->Period	= 65536;
		_Tim->Config();
	}
	
	if(_Port == NULL)
	{
		_Port = new	AlternatePort();
		_Port->Set(PA1);
		_Port->Open();
		_Port->AFConfig(GPIO_AF_2);
	}
	/* 使能自动装载 */
	TIM_ARRPreloadConfig(TIM2, ENABLE);
	// 捕获设置
	// 2通道映射到1通道   配套使用DMA1 Channel5
	TIM_ICInitTypeDef  TIM_ICInitStructure;  
	TIM_ICInitStructure.TIM_Channel		= TIM_Channel_1;
	TIM_ICInitStructure.TIM_ICPolarity	= TIM_ICPolarity_BothEdge;
	TIM_ICInitStructure.TIM_ICSelection	= TIM_ICSelection_IndirectTI;
	TIM_ICInitStructure.TIM_ICPrescaler	= TIM_ICPSC_DIV1;
	TIM_ICInitStructure.TIM_ICFilter = 0x0;
	TIM_PWMIConfig(TIM2, &TIM_ICInitStructure);
	
	/* Select the TIM2 Input Trigger: TI2FP2 */
	TIM_SelectInputTrigger(TIM2, TIM_TS_TI2FP2);
	/* 捕获的同时清空定时器计数 */
	// TIM_SelectSlaveMode(TIM2, TIM_SlaveMode_Reset);
	// TIM_SelectMasterSlaveMode(TIM2,TIM_MasterSlaveMode_Enable);
	// DMA 读取CNT寄存器，一字节设置  无法解决数据冗余
	// TIM_DMAConfig(TIM2, TIM_DMABase_CNT, TIM_DMABurstLength_1Transfer);
	// 连接DMA 触发机制
	TIM_DMACmd(TIM2, TIM_DMA_CC1, ENABLE);
	
	//TIM_Cmd(TIM2, ENABLE);
	
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1 , ENABLE);
	
	// 使用通道5
	/* DMA1 Channel1 Config */
	DMA_InitTypeDef     DMA_InitStructure;
	DMA_DeInit(DMA1_Channel5);
	DMA_InitStructure.DMA_PeripheralBaseAddr	= (uint32_t)&(TIM2->CNT);
	DMA_InitStructure.DMA_MemoryBaseAddr 		= (uint32_t)bs.GetBuffer();
	//DMA_InitStructure.DMA_MemoryBaseAddr 		= (uint32_t)buf;
	DMA_InitStructure.DMA_DIR 					= DMA_DIR_PeripheralSRC;
	DMA_InitStructure.DMA_BufferSize 			= DmaLen;
	DMA_InitStructure.DMA_PeripheralInc 		= DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc 			= DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize 	= DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure.DMA_MemoryDataSize 		= DMA_MemoryDataSize_HalfWord;
	DMA_InitStructure.DMA_Mode 					= DMA_Mode_Normal;
	DMA_InitStructure.DMA_Priority 				= DMA_Priority_High;
	DMA_InitStructure.DMA_M2M 					= DMA_M2M_Disable;
	DMA_Init(DMA1_Channel5, &DMA_InitStructure);
  
	// 开启DMA TIME
	DMA_Cmd(DMA1_Channel5, ENABLE);
	TIM_Cmd(TIM2, ENABLE);
	
	Stat = WaitRev;
	// 等待起始条件
	TimeWheel tw(sTimeout);
	while(!tw.Expired() && DMA_GetCurrDataCounter(DMA1_Channel5) == DmaLen);

	if(tw.Expired()) 	// 超时没有收到学习
	{	
		Stat = RevOver;
		// 关闭Time DMA
		DMA_Cmd(DMA1_Channel5, DISABLE);
		TIM_Cmd(TIM2, DISABLE);
		
		int length = DMA_GetCurrDataCounter(DMA1_Channel5);
		debug_printf("\r\n DMA::Counter %d \r\n", length);
		
		bs.SetLength(0);
		return -1;
	}
	// 进入接收状态
	Stat = Rev;

	debug_printf("\r\n开始接收数据\r\n");
	// 300ms 接收时间
	tw.Reset(0,400);
	while(!tw.Expired() && Stat != RevOver);
	
	// 关闭Time DMA	
	DMA_Cmd(DMA1_Channel5, DISABLE);
	TIM_Cmd(TIM2, DISABLE);
	
	uint len = DmaLen - DMA_GetCurrDataCounter(DMA1_Channel5);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, DISABLE);
	debug_printf("DMA REV %d byte\r\n",len);
	
	if(len > 30)
	{
		Stat = RevOver;
		
		ushort * p = (ushort *)bs.GetBuffer();
		for(int i = 0; i < len-1; i++)
		{
			if(p[i+1] > p[i])
				p[i] = p[i+1] - p[i];
			else
			{
				int a = p[i+1] + 0x10000;
				p[i] = a - p[i];
			}
		}

		len <<= 1;
		bs.SetLength(len);
		return len;
	}
	else
	{
		Stat = RevOver;
		bs.SetLength(0);
		return -1;
	}
#else
	return -1;
#endif
}
