#include "ADC.h"

#include "Platform\stm32.h"

Pin ADC_Pins[] = ADC1_PINS;

ADConverter::ADConverter(byte line, uint channel)
{
	assert_param2(line >= 1 && line <= 3, "ADC Line");

	Line	= line;
	Channel	= channel;

	uint dat	= 1;
	Count	= 0;
	for(int i=0; i<ArrayLength(Data); i++, dat <<= 1)
	{
		if(Channel & dat) Count++;
	}

	Buffer(Data, sizeof(Data)).Clear();

	ADC_TypeDef* const g_ADCs[]= ADCS;
	_ADC = g_ADCs[line - 1];
}

void ADConverter::Add(Pin pin)
{
	for(int i=0; i<ArrayLength(ADC_Pins); i++)
	{
		if(ADC_Pins[i] == pin)
		{
			Channel |= (1 << i);
			Count++;
			return;
		}
	}
}

void ADConverter::Remove(Pin pin)
{
	for(int i=0; i<ArrayLength(ADC_Pins); i++)
	{
		if(ADC_Pins[i] == pin)
		{
			Channel &= ~(1 << i);
			Count--;
			return;
		}
	}
}

void ADConverter::Open()
{
	debug_printf("ADC::Open %d 共%d个通道\r\n", Line, Count);

	auto at	= (ADC_TypeDef*)_ADC;
	/* Enable DMA clock */
#ifdef STM32F4
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);

	ADC_DeInit();
#else
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

	ADC_DeInit(at);
#endif
	/* Enable ADC and GPIOC clock */
	//RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_GPIOC, ENABLE);
	const int g_ADC_rccs[]= ADC_RCCS;
	RCC_APB2PeriphClockCmd(g_ADC_rccs[Line - 1], ENABLE);

	uint dat = 1;
	for(int i=0; i<ArrayLength(Data); i++, dat <<= 1)
	{
		if(Channel & dat)
		{
			debug_printf("ADC::Init %d ", i+1);
			if(i < ArrayLength(ADC_Pins))
				_ports[i].Set(ADC_Pins[i]).Open();
			else
				debug_printf("\r\n");
		}
	}

	/* DMA channel1 configuration */
#ifdef STM32F4
	DMA_DeInit(DMA1_Stream0);

	DMA_InitTypeDef dma;
	DMA_StructInit(&dma);
	dma.DMA_PeripheralBaseAddr = (uint)&at->DR;	 	//ADC地址
	//dma.DMA_MemoryBaseAddr = (uint)&Data;				//内存地址
	//dma.DMA_DIR = DMA_DIR_PeripheralSRC;
	dma.DMA_BufferSize = Count;
	dma.DMA_PeripheralInc = DMA_PeripheralInc_Disable;	//外设地址固定
	dma.DMA_MemoryInc = DMA_MemoryInc_Enable;  			//内存地址固定
	dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;	//半字
	dma.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	dma.DMA_Mode = DMA_Mode_Circular;								//循环传输
	dma.DMA_Priority = DMA_Priority_High;
	//dma.DMA_M2M = DMA_M2M_Disable;
	DMA_Init(DMA1_Stream0, &dma);

	/* Enable DMA channel1 */
	DMA_Cmd(DMA1_Stream0, ENABLE);
#else
	DMA_DeInit(DMA1_Channel1);

	DMA_InitTypeDef dma;
	DMA_StructInit(&dma);
	dma.DMA_PeripheralBaseAddr = (uint)&at->DR;	 	//ADC地址
	dma.DMA_MemoryBaseAddr = (uint)&Data;				//内存地址
	dma.DMA_DIR = DMA_DIR_PeripheralSRC;
	dma.DMA_BufferSize = Count;
	dma.DMA_PeripheralInc = DMA_PeripheralInc_Disable;	//外设地址固定
	dma.DMA_MemoryInc = DMA_MemoryInc_Enable;  			//内存地址固定
	dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;	//半字
	dma.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	dma.DMA_Mode = DMA_Mode_Circular;								//循环传输
	dma.DMA_Priority = DMA_Priority_High;
	dma.DMA_M2M = DMA_M2M_Disable;
	DMA_Init(DMA1_Channel1, &dma);

	/* Enable DMA channel1 */
	DMA_Cmd(DMA1_Channel1, ENABLE);
#endif

	/* ADC configuration */
	ADC_InitTypeDef adc;
	ADC_StructInit(&adc);

#if defined(STM32F1) || defined(GD32)
	adc.ADC_Mode = ADC_Mode_Independent;			//独立ADC模式
	adc.ADC_ScanConvMode = ENABLE ; 	 			//禁止扫描模式，扫描模式用于多通道采集
	adc.ADC_ContinuousConvMode = ENABLE;			//开启连续转换模式，即不停地进行ADC转换
	adc.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;	//不使用外部触发转换
	adc.ADC_DataAlign = ADC_DataAlign_Right; 		//采集数据右对齐
	adc.ADC_NbrOfChannel = Count;	 	//要转换的通道数目1
	ADC_Init(at, &adc);

#if defined(GD32)
	RCC_ADCCLKConfig(RCC_ADCCLK_APB2_DIV6);
#else
	/*配置ADC时钟，为PCLK2的8分频，即9MHz*/
	RCC_ADCCLKConfig(RCC_PCLK2_Div8);
#endif

	/*配置ADC1的通道10 11为55.	5个采样周期，序列为1 */
	//ADC_RegularChannelConfig(at, ADC_Channel_10, 1, ADC_SampleTime_55Cycles5);
	dat = 1;
	uint n = 1;
	for(byte i=0; i<ArrayLength(Data); i++, dat <<= 1)
	{
		if(Channel & dat)
		{
			// 第三个参数rank必须连续
			if(i < ADC_Channel_TempSensor)
				ADC_RegularChannelConfig(at, i, n++, ADC_SampleTime_55Cycles5);
			else
				ADC_RegularChannelConfig(at, i, n++, ADC_SampleTime_239Cycles5);
		}
	}
	if(Channel & 0x30000) ADC_TempSensorVrefintCmd(ENABLE);

	/* Enable ADC DMA */
	ADC_DMACmd(at, ENABLE);

	/* Enable ADC */
	ADC_Cmd(at, ENABLE);

	/*复位校准寄存器 */
	ADC_ResetCalibration(at);
	/*等待校准寄存器复位完成 */
	while(ADC_GetResetCalibrationStatus(at));

	/* ADC校准 */
	ADC_StartCalibration(at);
	/* 等待校准完成*/
	while(ADC_GetCalibrationStatus(at));

	/* 由于没有采用外部触发，所以使用软件触发ADC转换 */
	ADC_SoftwareStartConvCmd(at, ENABLE);
#elif defined(STM32F0)
	/* ADC DMA request in circular mode */
	ADC_DMARequestModeConfig(at, ADC_DMAMode_Circular);

	/* Enable ADC_DMA */
	ADC_DMACmd(at, ENABLE);

	/* Configure the ADC in continous mode withe a resolutuion equal to 12 bits  */
	adc.ADC_Resolution = ADC_Resolution_12b;
	adc.ADC_ContinuousConvMode = ENABLE;
	adc.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
	adc.ADC_DataAlign = ADC_DataAlign_Right;
	adc.ADC_ScanDirection = ADC_ScanDirection_Backward;
	ADC_Init(at, &adc);

	dat = 1;
	for(int i=0; i<ArrayLength(Data); i++, dat <<= 1)
	{
		if(Channel & dat)
		{
			ADC_ChannelConfig(at, dat , ADC_SampleTime_55_5Cycles);
			if(dat == ADC_Channel_TempSensor)
				ADC_TempSensorCmd(ENABLE);
			else if(dat == ADC_Channel_Vrefint)
				ADC_VrefintCmd(ENABLE);
			else if(dat == ADC_Channel_Vbat)
				ADC_VbatCmd(ENABLE);
		}
	}

	/* ADC Calibration */
	ADC_GetCalibrationFactor(at);

	/* Enable ADC */
	ADC_Cmd(at, ENABLE);

	// GD32F130需要20us左右的延时
	Sys.Delay(2000);

	/* Wait the ADCEN falg */
	while(!ADC_GetFlagStatus(at, ADC_FLAG_ADEN));

	/* ADC regular Software Start Conv */
	ADC_StartOfConversion(at);
#endif
}

ushort ADConverter::Read(Pin pin)
{
	ushort dat = 1;
	int n = 0;
	for(int i=0; i<ArrayLength(ADC_Pins); i++, dat <<= 1)
	{
		if(Channel & dat)
		{
			n++;
			if(ADC_Pins[i] == pin)
			{
				return Data[n-1];
			}
		}
	}
	return 0;
}

ushort ADConverter::ReadTempSensor()
{
	// 先判断有没有打开通道
#if defined(STM32F1) || defined(GD32)
	if(!(Channel & (1 << ADC_Channel_TempSensor))) return 0;
#elif defined(STM32F0)
	if(!(Channel & ADC_Channel_TempSensor)) return 0;
#endif

	ushort dat = 1;
	int n = 0;
	for(int i=0; i<ArrayLength(ADC_Pins); i++, dat <<= 1)
	{
		if(Channel & dat) n++;
	}
	return Data[n];
}

ushort ADConverter::ReadVrefint()
{
	// 先判断有没有打开通道
#if defined(STM32F1) || defined(GD32)
	if(!(Channel & (1 << ADC_Channel_Vrefint))) return 0;
#elif defined(STM32F0)
	if(!(Channel & ADC_Channel_Vrefint)) return 0;
#endif

	ushort dat = 1;
	int n = 0;
	for(int i=0; i<ArrayLength(ADC_Pins); i++, dat <<= 1)
	{
		if(Channel & dat) n++;
	}
#if defined(STM32F1) || defined(GD32)
	if(Channel & (1 << ADC_Channel_TempSensor)) n++;
#elif defined(STM32F0)
	if(Channel & ADC_Channel_TempSensor) n++;
#endif
	return Data[n];
}

#if defined(STM32F0) && !defined(GD32)
ushort ADConverter::ReadVbat()
{
	// 先判断有没有打开通道
	if(!(Channel & ADC_Channel_Vbat)) return 0;

	ushort dat = 1;
	int n = 0;
	for(int i=0; i<ArrayLength(ADC_Pins); i++, dat <<= 1)
	{
		if(Channel & dat) n++;
	}
	if(Channel & ADC_Channel_TempSensor) n++;
	if(Channel & ADC_Channel_Vrefint) n++;
	return Data[n];
}
#endif
