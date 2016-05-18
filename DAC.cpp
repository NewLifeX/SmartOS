#include "DAC.h"

#include "Platform\stm32.h"

#if defined(STM32F1)
DAConverter::DAConverter(Pin pin)
{
	if (pin == P0)
	{
		debug_printf("Pin不能为空");
		return;
	}
	if (pin == PA4)Channel = DAC_Channel_1;
	else Channel = DAC_Channel_2;

	Align = DAConverter::AlignR;

}

bool DAConverter::Open()
{
	// GPIO
	// DAC 变换器
	RCC_APB2PeriphClockCmd(RCC_APB1Periph_DAC, ENABLE);
	DAC_InitTypeDef adccfg;
	DAC_StructInit(&adccfg);
	//adccfg.DAC_Trigger = DAC_Trigger_None;				// 不使用外部触发
	adccfg.DAC_Trigger = DAC_Trigger_Software;				// 使用软件触发
	adccfg.DAC_WaveGeneration = DAC_WaveGeneration_None;	// 不使用波形发生器
	adccfg.DAC_LFSRUnmask_TriangleAmplitude = DAC_LFSRUnmask_Bit0;	// 屏蔽幅值设置
	adccfg.DAC_OutputBuffer = DISABLE;						// 不使用缓冲器
	DAC_Init(Channel, &adccfg);
	DAC_Cmd(Channel, ENABLE);
	return true;
}

bool DAConverter::Close()
{
	DAC_Cmd(Channel, DISABLE);
	RCC_APB2PeriphClockCmd(RCC_APB1Periph_DAC, DISABLE);
	return true;
}

bool DAConverter::Output(ushort dat)	// 处理对齐问题
{
	//if (Opened)return false;
	if (Channel == DAC_Channel_1)
		DAC_SetChannel1Data(Align, dat);
	else
		DAC_SetChannel2Data(Align, dat);
	DAC_SoftwareTriggerCmd(Channel, ENABLE);
	return true;
}

#endif
