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
    if(!rx) rx	= new AlternatePort(sp.Pins[1]);

    tx->Open();
	rx->Open();

	auto idx	= sp.Index;
	if (idx) { // COM2-5 on APB1
        RCC->APB1ENR |= RCC_APB1ENR_USART2EN >> 1 << idx;
    } else { // COM1 on APB2
        RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    }

	const byte afs[] = { GPIO_AF_USART1, GPIO_AF_USART2, GPIO_AF_USART3, GPIO_AF_UART4, GPIO_AF_UART5, GPIO_AF_USART6, GPIO_AF_UART7, GPIO_AF_UART8 };
	tx->AFConfig((Port::GPIO_AF)afs[idx]);
	rx->AFConfig((Port::GPIO_AF)afs[idx]);
}

// 获取引脚
void SerialPort_GetPins(byte index, byte remap, Pin* txPin, Pin* rxPin)
{
    *rxPin = *txPin = P0;

	const Pin g_Uart_Pins[] = UART_PINS;
	const Pin* p = g_Uart_Pins;

	int n = index << 2;
	*txPin  = p[n];
	*rxPin  = p[n + 1];
}
