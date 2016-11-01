#include "Device\Port.h"
#include "Device\DAC.h"

#include "Platform\stm32.h"

/*enum
{
	AlignR = DAC_Align_12b_R,
	AlignL = DAC_Align_12b_L,
	Alignsht = DAC_Align_8b_R
}Align;*/

#if defined(STM32F1)
void DAConverter::OnInit()
{
	if (_Pin == PA4)
		Channel	= DAC_Channel_1;
	else
		Channel	= DAC_Channel_2;

	Align	= DAC_Align_12b_R;
}

bool DAConverter::OnOpen()
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

bool DAConverter::OnClose()
{
	DAC_Cmd(Channel, DISABLE);
	RCC_APB2PeriphClockCmd(RCC_APB1Periph_DAC, DISABLE);

	return true;
}

bool DAConverter::OnWrite(ushort value)	// 处理对齐问题
{
	//if (Opened)return false;
	if (Channel == DAC_Channel_1)
		DAC_SetChannel1Data(Align, value);
	else
		DAC_SetChannel2Data(Align, value);
	DAC_SoftwareTriggerCmd(Channel, ENABLE);

	return true;
}

#endif
