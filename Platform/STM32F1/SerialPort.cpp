#include "Sys.h"
#include "Kernel\Task.h"
#include "Kernel\Interrupt.h"
#include "Device\SerialPort.h"
#include "Platform\stm32.h"

#define COM_DEBUG 0

const byte uart_irqs[] = UART_IRQs;

static void GetPins(byte index, byte remap, Pin* txPin, Pin* rxPin);

void SerialPort::OnInit()
{
    _parity		= USART_Parity_No;
    _dataBits	= USART_WordLength_8b;
    _stopBits	= USART_StopBits_1;
}

bool SerialPort::OnSet()
{
	USART_TypeDef* const g_Uart_Ports[] = UARTS;
	assert_param(_index < ArrayLength(g_Uart_Ports));

	GetPins(_index, Remap, &Pins[0], &Pins[1]);

    auto sp	= g_Uart_Ports[_index];
	_port	= sp;

	// 根据端口实际情况决定打开状态
	return sp->CR1 & USART_CR1_UE;
}

// 打开串口
void SerialPort::OnOpen2()
{
	// 串口引脚初始化
    if(!Ports[0]) Ports[0]	= new OutputPort(Pins[0]);
    if(!Ports[1]) Ports[1]	= new InputPort(Pins[1]);

	auto st	= (USART_TypeDef*)_port;

	// 不要关调试口，否则杯具
    if(_index != Sys.MessagePort) USART_DeInit(st);
	// USART_DeInit其实就是关闭时钟，这里有点多此一举。但为了安全起见，还是使用

	// 检查重映射
#ifdef STM32F1
	if(Remap)
	{
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
		switch (_index) {
		case 0: AFIO->MAPR |= AFIO_MAPR_USART1_REMAP; break;
		case 1: AFIO->MAPR |= AFIO_MAPR_USART2_REMAP; break;
		case 2: AFIO->MAPR |= AFIO_MAPR_USART3_REMAP_FULLREMAP; break;
		}
	}
#endif

    // 打开 UART 时钟。必须先打开串口时钟，才配置引脚
#if defined(STM32F0) || defined(GD32F150)
	switch(_index)
	{
		case COM1:	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);	break;
		case COM2:	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);	break;
		default:	break;
	}
#else
	if (_index) { // COM2-5 on APB1
        RCC->APB1ENR |= RCC_APB1ENR_USART2EN >> 1 << _index;
    } else { // COM1 on APB2
        RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    }
#endif

#if defined(STM32F0) || defined(GD32F150)
	_tx.AFConfig(Port::AF_1);
	_rx.AFConfig(Port::AF_1);
#elif defined(STM32F4)
	const byte afs[] = { GPIO_AF_USART1, GPIO_AF_USART2, GPIO_AF_USART3, GPIO_AF_UART4, GPIO_AF_UART5, GPIO_AF_USART6, GPIO_AF_UART7, GPIO_AF_UART8 };
	_tx.AFConfig((Port::GPIO_AF)afs[_index]);
	_rx.AFConfig((Port::GPIO_AF)afs[_index]);
#endif

	USART_InitTypeDef  p;
    USART_StructInit(&p);
	p.USART_BaudRate	= _baudRate;
	p.USART_WordLength	= _dataBits;
	p.USART_StopBits	= _stopBits;
	p.USART_Parity		= _parity;
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
	byte irq = uart_irqs[_index];
	Interrupt.SetPriority(irq, 0);
	Interrupt.Activate(irq, OnHandler, this);
//#endif

	USART_Cmd(st, ENABLE);//使能串口
}

// 关闭端口
void SerialPort::OnClose2()
{
	auto st	= (USART_TypeDef*)_port;
	USART_Cmd(st, DISABLE);
    USART_DeInit(st);

	//if(por)
    //_tx.Close();
	//_rx.Close();
	 if(Ports[0])
	 {
		 Ports[0]->Close();
		 delete Ports[0];
	 }
	 
	if(Ports[1]) 
	{
		Ports[1]->Close();
		delete Ports[1];
	}
	//const byte irqs[] = UART_IRQs;
	byte irq = uart_irqs[_index];
	Interrupt.Deactivate(irq);

	// 检查重映射
#ifdef STM32F1
	if(Remap)
	{
		switch (_index) {
		case 0: AFIO->MAPR &= ~AFIO_MAPR_USART1_REMAP; break;
		case 1: AFIO->MAPR &= ~AFIO_MAPR_USART2_REMAP; break;
		case 2: AFIO->MAPR &= ~AFIO_MAPR_USART3_REMAP_FULLREMAP; break;
		}
	}
#endif
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
	auto st	= (USART_TypeDef*)_port;
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
	USART_ITConfig((USART_TypeDef*)_port, USART_IT_TXE, ENABLE);
}

#if !defined(TINY) && defined(STM32F0)
	#pragma arm section code = "SectionForSys"
#endif

void SerialPort::OnTxHandler()
{
	if(!Tx.Empty())
		USART_SendData((USART_TypeDef*)_port, (ushort)Tx.Pop());
	else
	{
		USART_ITConfig((USART_TypeDef*)_port, USART_IT_TXE, DISABLE);

		Set485(false);
	}
}

void SerialPort::OnRxHandler()
{
	// 串口接收中断必须以极快的速度完成，否则会出现丢数据的情况
	// 判断缓冲区足够最小值以后才唤醒任务，减少时间消耗
	// 缓冲区里面别用%，那会产生非常耗时的除法运算
	byte dat = (byte)USART_ReceiveData((USART_TypeDef*)_port);
	Rx.Push(dat);

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
	auto st	= (USART_TypeDef*)sp->_port;

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
		//debug_printf("Serial%d 溢出 \r\n", sp->_index + 1);
	}
	/*if(USART_GetFlagStatus(st, USART_FLAG_NE) != RESET) USART_ClearFlag(st, USART_FLAG_NE);
	if(USART_GetFlagStatus(st, USART_FLAG_FE) != RESET) USART_ClearFlag(st, USART_FLAG_FE);
	if(USART_GetFlagStatus(st, USART_FLAG_PE) != RESET) USART_ClearFlag(st, USART_FLAG_PE);*/
}

#pragma arm section code

// 获取引脚
void GetPins(byte index, byte remap, Pin* txPin, Pin* rxPin)
{
    *rxPin = *txPin = P0;

	const Pin g_Uart_Pins[] = UART_PINS;
	const Pin* p = g_Uart_Pins;
#ifdef STM32F1
	const Pin g_Uart_Pins_Map[] = UART_PINS_FULLREMAP;
	if(remap) p = g_Uart_Pins_Map;
#endif

	int n = index << 2;
	*txPin  = p[n];
	*rxPin  = p[n + 1];
}

extern "C"
{
    extern SerialPort* _printf_sp;
	bool isInFPutc;

    /* 重载fputc可以让用户程序使用printf函数 */
    int fputc(int ch, FILE *f)
    {
#if DEBUG
        if(Sys.Clock == 0) return ch;

        int idx	= Sys.MessagePort;
        if(idx == COM_NONE) return ch;

		if(isInFPutc) return ch;
		isInFPutc = true;

		USART_TypeDef* g_Uart_Ports[] = UARTS;
        auto port = g_Uart_Ports[idx];

        // 检查并打开串口
        if((port->CR1 & USART_CR1_UE) != USART_CR1_UE)
        {
            _printf_sp = SerialPort::GetMessagePort();
        }

		if(_printf_sp)
		{
			byte b = ch;
			_printf_sp->Write(Buffer(&b, 1));
		}

		isInFPutc = false;
#endif
        return ch;
    }
}
