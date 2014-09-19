#include "RTC.h"



//	InterruptCallback RCC_Handle;
//  typedef void (*InterruptCallback)(ushort num, void* param);
//  我去 还得用全局函数
void RCC_Handler(ushort num, void* param);



RTClock::RTClock()
{
	_Handler = NULL;
	_param = NULL;
	_TickTime = 0;
}

RTClock::RTClock(int s)
{
	_Handler = NULL;
	_param = NULL;
	_TickTime = s;
}


void RTClock::Register(RTCHandler handler, void* param)
{
	PWR_BackupAccessCmd(ENABLE);		// 使能访问BKP区    RTC配置寄存器在BKP内

	RCC_LSICmd( ENABLE );				// 节点上使用LSI
	while(RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET);		// 等待LSI
	RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);					// 多路器选择LSI 为RTC提供时钟
	RCC_RTCCLKCmd( ENABLE );								// 允许TRC时钟
	
	RTC_DeInit();											// 复位
	RTC_WaitForSynchro();									// 等待RTC - APB同步		
	
	
//void RCC_BackupResetCmd(FunctionalState NewState);
	
//	RTC_ITConfig(uint32_t RTC_IT, FunctionalState NewState);
	Interrupt.SetPriority(RCC_IRQn, 1);
	if(param != NULL)
	{
		_Handler = handler;
		_param = param;
		Interrupt.Activate(RCC_IRQn,RCC_Handler,this);
	}
}

void RCC_Handler(ushort num, void* param)
{
	RTClock * rtc = (RTClock *)param;
	if(param != NULL)
		rtc->_Handler(rtc->_param);
}



