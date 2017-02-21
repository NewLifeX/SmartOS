#include "Kernel\Sys.h"
#include "Kernel\Task.h"
#include "Kernel\Interrupt.h"
#include "Device\SerialPort.h"
#include "Platform\stm32.h"

#define COM_DEBUG 0

// 打开串口
void SerialPort_Opening(SerialPort& sp)
{
	auto& tx	= sp.Ports[0];
	auto& rx	= sp.Ports[1];

	// 串口引脚初始化
    if(!tx) tx	= new AlternatePort(sp.Pins[0]);
    if(!rx)
	{
		auto port	= new InputPort();
		port->Init(sp.Pins[1], false).Open();
		rx	= port;
	}

    tx->Open();
	rx->Open();

	auto idx	= sp.Index;
	// 检查重映射
	if(sp.Remap)
	{
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
		switch (idx) {
		case 0: AFIO->MAPR |= AFIO_MAPR_USART1_REMAP; break;
		case 1: AFIO->MAPR |= AFIO_MAPR_USART2_REMAP; break;
		case 2: AFIO->MAPR |= AFIO_MAPR_USART3_REMAP_FULLREMAP; break;
		}
	}

    // 打开 UART 时钟。必须先打开串口时钟，才配置引脚
	if (idx) { // COM2-5 on APB1
        RCC->APB1ENR |= RCC_APB1ENR_USART2EN >> 1 << idx;
    } else { // COM1 on APB2
        RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    }
}

// 关闭端口
void SerialPort_Closeing(SerialPort& sp)
{
	// 检查重映射
	if(sp.Remap)
	{
		switch (sp.Index) {
		case 0: AFIO->MAPR &= ~AFIO_MAPR_USART1_REMAP; break;
		case 1: AFIO->MAPR &= ~AFIO_MAPR_USART2_REMAP; break;
		case 2: AFIO->MAPR &= ~AFIO_MAPR_USART3_REMAP_FULLREMAP; break;
		}
	}
}

// 获取引脚
void SerialPort_GetPins(byte index, byte remap, Pin* txPin, Pin* rxPin)
{
    *rxPin = *txPin = P0;

	const Pin g_Uart_Pins[] = UART_PINS;
	const Pin* p = g_Uart_Pins;
	const Pin g_Uart_Pins_Map[] = UART_PINS_FULLREMAP;
	if(remap) p = g_Uart_Pins_Map;

	int n = index << 2;
	*txPin  = p[n];
	*rxPin  = p[n + 1];
}
