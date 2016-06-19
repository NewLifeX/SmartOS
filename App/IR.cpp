#include "IR.h"
#include "Time.h"

//#include "Platform\stm32.h"

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

ushort * SendP = nullptr;
ushort  SendIndex;
ushort SendBufLen;
bool ErrorIRQ;

bool IR::Send(const Buffer& bs)
{
#ifdef STM32F0
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	TIM_DeInit(TIM2);

	if(_Tim == nullptr)
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

	// 暂时避过 定时器一启动立马中断的错误
	ErrorIRQ = true;
	// 开始  中断内处理数据
	// _Pwm->Open();
	_Tim->Open();

	// 等待结束
	_SendOK	= false;
	WaitHandle wh;
	wh.WaitOne(2000, _SendOK);

	// 结束 不论是超时还是发送结束都关闭pwm和定时器
	//_Pwm->Close();
	_Tim->Register(nullptr,nullptr);
	_Tim->Close();
	// 清空使用的数据
	SendIndex = 0;
	SendP = nullptr;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, DISABLE);
#endif
	return true;
}

void IR::OnSend(void* sender, void* param)
{
	//TS("IR::OnSend");
#ifdef STM32F0
	auto ir = (IR*)param;
	auto ti	= (TIM_TypeDef*)(ir->_Pwm->_Timer);
	if(ErrorIRQ)	// 避开打开定时器立马中断问题
	{
		ErrorIRQ = false;
		ir->_Pwm->Open();
		TIM_Cmd(ti, ENABLE);
		//debug_printf("pwm open\r\n");
		return;
	}
	// PWM归位
	TIM_SetCounter(ti, 0x00000000);
	// PWM变化
	//if(SendIndex % 2)
	if(SendIndex & 1)	//不行  暂不知问题所在
	{
		TIM_Cmd(ti, ENABLE);
	}
	else
	{
		TIM_Cmd(ti, DISABLE);
	}
	// 下一个周期
	SendIndex += 1;

	auto ti2	= (TIM_TypeDef*)(ir->_Tim->_Timer);
	TIM_SetAutoreload(ti2, SendP[SendIndex]);

	if(SendIndex >= SendBufLen)
	{
		// 发送完毕
		TIM_SetCounter(ti, 0x00000000);
		TIM_Cmd(ti, DISABLE);
		TIM_Cmd(ti2, DISABLE);
		//ir->_Pwm->Close();
		//ir->_Tim->Close();
		Stat = SendOver;
		_SendOK	= true;

		return;
	}
#endif
}

int IR::Receive(Buffer& bs, int sTimeout)
{
	TS("IR::Receive");
#ifdef STM32F0
	uint bufLen	= bs.Length();
	uint DmaLen	= bufLen/2 -1;

	// 捕获引脚初始化
	if(_Port == nullptr)
	{
		_Port = new	AlternatePort();	// 在括号内直接写引脚会初始化失败
		_Port->Set(PA1);
		_Port->Open();
		_Port->AFConfig(Port::AF_2);
	}
	if(_Tim == nullptr) _Tim = Timer::Create(0x01);		// 直接占用TIMER2

	// RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, DISABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	// TIM_DeInit(TIM2);

	// _Tim->SetFrequency(50);	//Prescaler  Period 需要定制
	_Tim->Prescaler	= 400;
	// 波形间隔悬殊，用一个字节误差太大
	_Tim->Period	= 65536;
	_Tim->Config();

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
	/* 捕获的同时清空定时器计数，不能使用 这个复位是以电平为准，不是沿
	   使用会使采集到的数据一半为零    后期数据处理时候做差就好  */
	// TIM_SelectSlaveMode(TIM2, TIM_SlaveMode_Reset);
	// TIM_SelectMasterSlaveMode(TIM2,TIM_MasterSlaveMode_Enable);
	// DMA 读取CNT寄存器，一字节设置  无法解决数据冗余
	// TIM_DMAConfig(TIM2, TIM_DMABase_CNT, TIM_DMABurstLength_1Transfer);
	//TIM_Cmd(TIM2, ENABLE);

	// DMA 时钟
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1 , ENABLE);
	// 使用通道5
	/* DMA1 Channel1 Config */
	DMA_InitTypeDef     DMA_InitStructure;
	DMA_DeInit(DMA1_Channel5);
	DMA_DeInit(DMA1_Channel5);
	DMA_InitStructure.DMA_PeripheralBaseAddr	= (uint32_t)&(TIM2->CNT);
	DMA_InitStructure.DMA_MemoryBaseAddr 		= (uint32_t)bs.GetBuffer();
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
	// 连接DMA 触发机制
	TIM_DMACmd(TIM2, TIM_DMA_CC1, ENABLE);

	// 等待起始条件，DMA 开始搬运数据
	Stat = WaitRev;

	TimeWheel tw(sTimeout);
	tw.Sleep	= 50;
	while(!tw.Expired() && DMA_GetCurrDataCounter(DMA1_Channel5) == DmaLen);

	if(tw.Expired())
	{
		// 超时没有收到学习
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
	// 400ms 接收时间   以空调为准 180ms  翻倍基本可以罩住所有类型
	// 不能使用 定时器 中断作为超时依据  只能等。。
	tw.Reset(0,400);
	while(!tw.Expired()) Sys.Sleep(50);

	// 接收结束
	// 关闭Time DMA
	DMA_Cmd(DMA1_Channel5, DISABLE);
	TIM_Cmd(TIM2, DISABLE);
	// 本次接收结束，关闭 RCC  防止这些配置贻害无穷
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, DISABLE);
	// 拿到数据长度
	uint len = DmaLen - DMA_GetCurrDataCounter(DMA1_Channel5);
	debug_printf("DMA REV %d byte\r\n",len);

	if(len > 30)
	{
		if(len >= DmaLen)
		{
			bs.SetLength(0);
			return -1;
		}
		Stat = RevOver;
		// 处理数据，将绝对数 变为相对。便于发送时候使用
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
