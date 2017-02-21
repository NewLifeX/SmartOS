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

    // 打开 UART 时钟。必须先打开串口时钟，才配置引脚
	switch(sp.Index)
	{
		case COM1:	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);	break;
		case COM2:	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);	break;
		default:	break;
	}

	tx->AFConfig(Port::AF_1);
	rx->AFConfig(Port::AF_1);
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
