#include "Kernel\Sys.h"
#include "Device\DMA.h"

#include "Platform\stm32.h"

bool DMA::Start()
{
#ifdef STM32F4
	DMA_InitTypeDef  DMA_InitStructure;
	__IO uint32_t    Timeout = TIMEOUT_MAX;

	/* Enable DMA clock */
	RCC_AHB1PeriphClockCmd(DMA_STREAM_CLOCK, ENABLE);

	/* Reset DMA Stream registers (for debug purpose) */
	DMA_DeInit(DMA_STREAM);

	/* Check if the DMA Stream is disabled before enabling it.
	 Note that this step is useful when the same Stream is used multiple times:
	 enabled, then disabled then re-enabled... In this case, the DMA Stream disable
	 will be effective only at the end of the ongoing data transfer and it will
	 not be possible to re-configure it before making sure that the Enable bit
	 has been cleared by hardware. If the Stream is used only once, this step might
	 be bypassed. */
	while (DMA_GetCmdStatus(DMA_STREAM) != DISABLE)
	{
	}

	/* Configure DMA Stream */
	DMA_InitStructure.DMA_Channel = DMA_CHANNEL;
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)SRC_Const_Buffer;
	DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)DST_Buffer;
	DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToMemory;
	DMA_InitStructure.DMA_BufferSize = (uint32_t)BUFFER_SIZE;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Enable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
	DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
	DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
	DMA_Init(DMA_STREAM, &DMA_InitStructure);

	/* Enable DMA Stream Transfer Complete interrupt */
	DMA_ITConfig(DMA_STREAM, DMA_IT_TC, ENABLE);

	/* DMA Stream enable */
	DMA_Cmd(DMA_STREAM, ENABLE);

	/* Check if the DMA Stream has been effectively enabled.
	 The DMA Stream Enable bit is cleared immediately by hardware if there is an
	 error in the configuration parameters and the transfer is no started (ie. when
	 wrong FIFO threshold is configured ...) */
	Timeout = TIMEOUT_MAX;
	while ((DMA_GetCmdStatus(DMA_STREAM) != ENABLE) && (Timeout-- > 0))
	{
	}

	/* Check if a timeout condition occurred */
	if (Timeout == 0)
	{
		/* Manage the error: to simplify the code enter an infinite loop */
		while (1)
		{
		}
	}

	Interrupt.Enable(DMA_STREAM_IRQ);
#endif

	return true;
}

bool DMA::WaitForStop()
{
#ifdef STM32F4
	uint retry = Retry;
	//while (DMA_GetCurrentMemoryTarget(DMA_STREAM) != 0)
	while (DMA_GetCmdStatus(DMA_STREAM) != DISABLE)
	{
		if(--retry <= 0)
		{
			Error++;
			return false;
		}
	}
#endif
	return true;
}
