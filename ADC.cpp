#include "ADC.h"
#include "Port.h"

Pin ADC_Pins[] = ADC1_PINS;

ADConverter::ADConverter(byte line, ushort channel)
{
	assert_param(line >= 1 && line <= 3);

	Line	= line;
	Channel	= channel;

	ArrayZero(Data);

	ADC_TypeDef* const g_ADCs[]= {ADC1, ADC2, ADC3};
	_ADC = g_ADCs[line - 1];
}

void ADConverter::Open()
{
	debug_printf("ADC::Open %d\r\n", Line);

	/* Enable DMA clock */
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

	/* Enable ADC1 and GPIOC clock */
	//RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_GPIOC, ENABLE);
	const int g_ADC_rccs[]= {RCC_APB2Periph_ADC1, RCC_APB2Periph_ADC2, RCC_APB2Periph_ADC3};
	RCC_APB2PeriphClockCmd(g_ADC_rccs[Line - 1], ENABLE);

	ushort dat = 1;
	for(int i=0; i<16; i++, dat <<= 1)
	{
		if(Channel & dat)
		{
			debug_printf("ADC::Init %d ", i+1);
			AnalogInPort ai(ADC_Pins[i]);
		}
	}

	DMA_InitTypeDef dma;
	ADC_InitTypeDef adc;

	/* DMA channel1 configuration */
	DMA_DeInit(DMA1_Channel1);

	dma.DMA_PeripheralBaseAddr = (uint)&_ADC->DR;	 	//ADC地址
	dma.DMA_MemoryBaseAddr = (uint)&Data;				//内存地址
	dma.DMA_DIR = DMA_DIR_PeripheralSRC;
	dma.DMA_BufferSize = 2;
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

	/* ADC1 configuration */
	adc.ADC_Mode = ADC_Mode_Independent;			//独立ADC模式
	adc.ADC_ScanConvMode = ENABLE ; 	 			//禁止扫描模式，扫描模式用于多通道采集
	adc.ADC_ContinuousConvMode = ENABLE;			//开启连续转换模式，即不停地进行ADC转换
	adc.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;	//不使用外部触发转换
	adc.ADC_DataAlign = ADC_DataAlign_Right; 		//采集数据右对齐
	adc.ADC_NbrOfChannel = 2;	 	//要转换的通道数目1
	ADC_Init(_ADC, &adc);

	/*配置ADC时钟，为PCLK2的8分频，即9MHz*/
	RCC_ADCCLKConfig(RCC_PCLK2_Div8);
	/*配置ADC1的通道10 11为55.	5个采样周期，序列为1 */
	//ADC_RegularChannelConfig(_ADC, ADC_Channel_10, 1, ADC_SampleTime_55Cycles5);
	dat = 1;
	uint n = 1;
	for(byte i=0; i<0x10; i++, dat <<= 1)
	{
		if(Channel & dat)
		{
			// 第三个参数rank必须连续
			ADC_RegularChannelConfig(_ADC, i, n++, ADC_SampleTime_55Cycles5);
		}
	}

	/* Enable _ADC DMA */
	ADC_DMACmd(_ADC, ENABLE);

	/* Enable _ADC */
	ADC_Cmd(_ADC, ENABLE);

	/*复位校准寄存器 */
	ADC_ResetCalibration(_ADC);
	/*等待校准寄存器复位完成 */
	while(ADC_GetResetCalibrationStatus(_ADC));

	/* ADC校准 */
	ADC_StartCalibration(_ADC);
	/* 等待校准完成*/
	while(ADC_GetCalibrationStatus(_ADC));

	/* 由于没有采用外部触发，所以使用软件触发ADC转换 */
	ADC_SoftwareStartConvCmd(_ADC, ENABLE);
}
