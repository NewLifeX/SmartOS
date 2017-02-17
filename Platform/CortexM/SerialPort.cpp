#include "Kernel\Sys.h"
#include "Kernel\Task.h"
#include "Kernel\Interrupt.h"
#include "Device\SerialPort.h"
#include "Platform\stm32.h"

#define COM_DEBUG 0

const byte uart_irqs[] = UART_IRQs;

extern void SerialPort_GetPins(byte index, byte remap, Pin* txPin, Pin* rxPin);

void SerialPort::OnInit()
{
    /*_parity		= USART_Parity_No;
    _dataBits	= USART_WordLength_8b;
    _stopBits	= USART_StopBits_1;*/
    _dataBits	= 8;
    _parity		= 0;
    _stopBits	= 1;
}

bool SerialPort::OnSet()
{
	USART_TypeDef* const g_Uart_Ports[] = UARTS;
	assert_param(Index < ArrayLength(g_Uart_Ports));

	SerialPort_GetPins(Index, Remap, &Pins[0], &Pins[1]);

    auto sp	= g_Uart_Ports[Index];
	State	= sp;

	// 根据端口实际情况决定打开状态
	return sp->CR1 & USART_CR1_UE;
}

WEAK void SerialPort_Opening(SerialPort& sp)	{ }
WEAK void SerialPort_Closeing(SerialPort& sp)	{ }

// 打开串口
void SerialPort::OnOpen2()
{
	auto st	= (USART_TypeDef*)State;

	// 不要关调试口，否则杯具
    if(Index != Sys.MessagePort) USART_DeInit(st);
	// USART_DeInit其实就是关闭时钟，这里有点多此一举。但为了安全起见，还是使用

	SerialPort_Opening(*this);

	const ushort paritys[]	= { USART_Parity_No, USART_Parity_Even, USART_Parity_Odd };
	if(_parity >= ArrayLength(paritys)) _parity	= 0;

#ifdef STM32F0
	const ushort StopBits[]	= { 1, USART_StopBits_1, 2, USART_StopBits_2, 15, USART_StopBits_1_5 };
#else
	const ushort StopBits[]	= { 1, USART_StopBits_1, 5, USART_StopBits_0_5, 2, USART_StopBits_2, 15, USART_StopBits_1_5 };
#endif
	ushort stop	= StopBits[1];
	for(int i=0; i<ArrayLength(StopBits); i+=2)
	{
		if(StopBits[i] == _stopBits)
		{
			stop	= StopBits[i+1];
			break;
		}
	}

	USART_InitTypeDef  p;
    USART_StructInit(&p);
	p.USART_BaudRate	= _baudRate;
	p.USART_WordLength	= _dataBits == 8 ? USART_WordLength_8b : USART_WordLength_9b;
	p.USART_StopBits	= stop;
	p.USART_Parity		= paritys[_parity];
	USART_Init(st, &p);

	// 串口接收中断配置，同时会打开过载错误中断
	USART_ITConfig(st, USART_IT_RXNE, ENABLE);
	//USART_ITConfig(st, USART_IT_PE, ENABLE);
	//USART_ITConfig(st, USART_IT_ERR, ENABLE);
	//USART_ITConfig(st, USART_IT_TXE, DISABLE);

//#if defined(STM32F0) || defined(GD32F150)
	// GD官方提供，因GD设计比ST严格，导致一些干扰被错误认为是溢出
	//USART_OverrunDetectionConfig(st, USART_OVRDetection_Disable);
//#else
	// 打开中断，收发都要使用
	//const byte irqs[] = UART_IRQs;
	byte irq = uart_irqs[Index];
	Interrupt.SetPriority(irq, 0);
	Interrupt.Activate(irq, OnHandler, this);
//#endif

	USART_Cmd(st, ENABLE);//使能串口
}

// 关闭端口
void SerialPort::OnClose2()
{
	auto st	= (USART_TypeDef*)State;
	USART_Cmd(st, DISABLE);
    USART_DeInit(st);

    Ports[0]->Close();
	Ports[1]->Close();

	byte irq = uart_irqs[Index];
	Interrupt.Deactivate(irq);

	SerialPort_Closeing(*this);
}

// 发送单一字节数据
uint SerialPort::SendData(byte data, uint times)
{
	/*
	在USART_DR寄存器中写入了最后一个数据字后，在关闭USART模块之前或设置微控制器进入低功耗模式之前，
	必须先等待TC=1。使用下列软件过程清除TC位：
	1．读一次USART_SR寄存器；
	2．写一次USART_DR寄存器。
	*/
	auto st	= (USART_TypeDef*)State;
	USART_SendData(st, (ushort)data);
	// 等待发送完毕
    while(USART_GetFlagStatus(st, USART_FLAG_TXE) == RESET && --times > 0);
    if(!times) Error++;

	return times;
}

// 向某个端口写入数据。如果size为0，则把data当作字符串，一直发送直到遇到\0为止
void SerialPort::OnWrite2()
{
	// 打开串口发送
	USART_ITConfig((USART_TypeDef*)State, USART_IT_TXE, ENABLE);
}

#if !defined(TINY) && defined(STM32F0)
	#pragma arm section code = "SectionForSys"
#endif

void SerialPort::OnTxHandler()
{
	if(!Tx.Empty())
		USART_SendData((USART_TypeDef*)State, (ushort)Tx.Dequeue());
	else
	{
		USART_ITConfig((USART_TypeDef*)State, USART_IT_TXE, DISABLE);

		Set485(false);
	}
}

void SerialPort::OnRxHandler()
{
	// 串口接收中断必须以极快的速度完成，否则会出现丢数据的情况
	// 判断缓冲区足够最小值以后才唤醒任务，减少时间消耗
	// 缓冲区里面别用%，那会产生非常耗时的除法运算
	byte dat = (byte)USART_ReceiveData((USART_TypeDef*)State);
	Rx.Enqueue(dat);

	// 收到数据，开启任务调度。延迟_byteTime，可能还有字节到来
	//!!! 暂时注释任务唤醒，避免丢数据问题
	if(_taskidRx && Rx.Length() >= MinSize)
	{
		//Sys.SetTask(_taskidRx, true, (ByteTime >> 10) + 1);
		((Task*)_task)->Set(true, 20);
	}
}

// 真正的串口中断函数
void SerialPort::OnHandler(ushort num, void* param)
{
	auto sp	= (SerialPort*)param;
	auto st	= (USART_TypeDef*)sp->State;

//#if !(defined(STM32F0) || defined(GD32F150))
	if(USART_GetITStatus(st, USART_IT_TXE) != RESET) sp->OnTxHandler();
//#endif
	// 接收中断
	if(USART_GetITStatus(st, USART_IT_RXNE) != RESET) sp->OnRxHandler();
	// 溢出
	if(USART_GetFlagStatus(st, USART_FLAG_ORE) != RESET)
	{
		//USART_ClearFlag(st, USART_FLAG_ORE);	// ST 库文件 ClearFlag 不许动 USART_FLAG_ORE 寄存器
		// 读取并扔到错误数据
		USART_ReceiveData(st);
		sp->Error++;
		//debug_printf("Serial%d 溢出 \r\n", sp->Index + 1);
	}
	/*if(USART_GetFlagStatus(st, USART_FLAG_NE) != RESET) USART_ClearFlag(st, USART_FLAG_NE);
	if(USART_GetFlagStatus(st, USART_FLAG_FE) != RESET) USART_ClearFlag(st, USART_FLAG_FE);
	if(USART_GetFlagStatus(st, USART_FLAG_PE) != RESET) USART_ClearFlag(st, USART_FLAG_PE);*/
}

#pragma arm section code
