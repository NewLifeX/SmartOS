#include "IR.h"
#include "Time.h"
#include "stm32f0xx_tim.h"

/*
Timer2  CH2       通道
DMA1    Channel3  通道

*/

IR::IR()
{

}

bool IR::Open()
{
	return true;
}

bool IR::Close()
{
	return true;
}

bool IR::Send(const Array& bs)
{
	return true;
}

void IR::OnSend()
{

}

typedef enum
{
	WaitRev,
	Rev,
	RevOver,
}IRStat;

IRStat Stat;

int IR::Receive(Array& bs, int sTimeout)
{
	TS("IR::Receive");
	
	uint bufLen	= bs.Length();
	
	if(_Tim == NULL)
	{
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
		_Tim = Timer::Create(0x01);		// 直接占用TIMER2
		// _Tim->SetFrequency(50);		// 5Hz   20ms
		// 使用一个字节  需要自行配置分频系数
		_Tim->Prescaler	= 800;		// 分频 需要结果是 Period < 256 时能计数 20ms
		_Tim->Period	= 255;
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
	TIM_SelectSlaveMode(TIM2, TIM_SlaveMode_Reset);
	//TIM_SelectMasterSlaveMode(TIM2,TIM_MasterSlaveMode_Enable);
	
	//TIM_Cmd(TIM2, ENABLE);
	
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1 , ENABLE);
	// 连接DMA 触发机制
	TIM_DMACmd(TIM2, TIM_DMA_CC1, ENABLE);
	
	// 使用通道5
	/* DMA1 Channel1 Config */
	DMA_InitTypeDef     DMA_InitStructure;
	DMA_DeInit(DMA1_Channel5);
	DMA_InitStructure.DMA_PeripheralBaseAddr	= (uint32_t)&(TIM2->CNT);
	DMA_InitStructure.DMA_MemoryBaseAddr 		= (uint32_t)bs.GetBuffer();
	//DMA_InitStructure.DMA_MemoryBaseAddr 		= (uint32_t)buf;
	DMA_InitStructure.DMA_DIR 					= DMA_DIR_PeripheralSRC;
	DMA_InitStructure.DMA_BufferSize 			= bufLen;
	DMA_InitStructure.DMA_PeripheralInc 		= DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc 			= DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize 	= DMA_PeripheralDataSize_Byte;
	DMA_InitStructure.DMA_MemoryDataSize 		= DMA_MemoryDataSize_Byte;
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
	while(!tw.Expired() && DMA_GetCurrDataCounter(DMA1_Channel5) == bufLen);

	if(tw.Expired()) 	// 超时没有收到学习
	{	
		Stat = RevOver;
		// 关闭Time DMA
		DMA_Cmd(DMA1_Channel5, DISABLE);
		TIM_Cmd(TIM2, DISABLE);
		
		int leng = DMA_GetCurrDataCounter(DMA1_Channel5);
		debug_printf("\r\n DMA::Counter %d \r\n", leng);
		
		bs.SetLength(0);
		return -1;
	}
	// 进入接收状态
	Stat = Rev;

	debug_printf("\r\n开始接收数据\r\n");
	// 300ms 接收时间
	tw.Reset(1);
	while(!tw.Expired() && Stat != RevOver);
	
	// 关闭Time DMA	
	DMA_Cmd(DMA1_Channel5, DISABLE);
	TIM_Cmd(TIM2, DISABLE);
	
	int len = bufLen - DMA_GetCurrDataCounter(DMA1_Channel5);
	if(len > 30)
	{
		debug_printf("收到数据 %d byte\r\n",len);
		Stat = RevOver;
		bs.SetLength(len);
		return len;
	}
	else
	{
		debug_printf("未收到数据 %d byte\r\n",len);
		Stat = RevOver;
		bs.SetLength(0);
		return -1;
	}
}

void IR::OnReceive(void* sender, void* param)
{
}
