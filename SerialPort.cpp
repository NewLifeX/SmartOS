#include "Sys.h"
#include <stdio.h>

#include "Port.h"
#include "SerialPort.h"

#define COM_DEBUG 0

// 默认波特率
//#define USART_DEFAULT_BAUDRATE 115200

//static USART_TypeDef* const g_Uart_Ports[] = UARTS;
//static const Pin g_Uart_Pins[] = UART_PINS;
//static const Pin g_Uart_Pins_Map[] = UART_PINS_FULLREMAP;

SerialPort::SerialPort(USART_TypeDef* com, int baudRate, int parity, int dataBits, int stopBits)
{
	assert_param(com);

	const USART_TypeDef* const g_Uart_Ports[] = UARTS;
	byte _index = 0xFF;
	for(int i=0; i<ArrayLength(g_Uart_Ports); i++)
	{
		if(g_Uart_Ports[i] == com)
		{
			_index = i;
			break;
		}
	}

	Init(_index, baudRate, parity, dataBits, stopBits);
}

void SerialPort::Init(byte index, int baudRate, int parity, int dataBits, int stopBits)
{
	USART_TypeDef* const g_Uart_Ports[] = UARTS;
	_index = index;
	assert_param(_index < ArrayLength(g_Uart_Ports));

    _port = g_Uart_Ports[_index];
    _baudRate = baudRate;
    _parity = parity;
    _dataBits = dataBits;
    _stopBits = stopBits;

	_tx = NULL;
	_rx = NULL;
	RS485 = NULL;

	IsRemap = false;

	// 根据端口实际情况决定打开状态
	if(_port->CR1 & USART_CR1_UE) Opened = true;
}

// 析构时自动关闭
SerialPort::~SerialPort()
{
	if(RS485) delete RS485;
	RS485 = NULL;
}

// 打开串口
bool SerialPort::OnOpen()
{
    Pin rx, tx;
    GetPins(&tx, &rx);

    //debug_printf("Serial%d Open(%d, %d, %d, %d)\r\n", _index + 1, _baudRate, _parity, _dataBits, _stopBits);
#if COM_DEBUG
    if(_index != Sys.MessagePort)
    {
ShowLog:
        debug_printf("Serial%d Open(%d", _index + 1, _baudRate);
        switch(_parity)
        {
            case USART_Parity_No: debug_printf(", Parity_None"); break;
            case USART_Parity_Even: debug_printf(", Parity_Even"); break;
            case USART_Parity_Odd: debug_printf(", Parity_Odd"); break;
        }
        switch(_dataBits)
        {
            case USART_WordLength_8b: debug_printf(", WordLength_8b"); break;
            case USART_WordLength_9b: debug_printf(", WordLength_9b"); break;
        }
        switch(_stopBits)
        {
#ifdef STM32F10X
            case USART_StopBits_0_5: debug_printf(", StopBits_0_5"); break;
#endif
            case USART_StopBits_1: debug_printf(", StopBits_1"); break;
            case USART_StopBits_1_5: debug_printf(", StopBits_1_5"); break;
            case USART_StopBits_2: debug_printf(", StopBits_2"); break;
        }
        debug_printf(") TX=P%c%d RX=P%c%d\r\n", _PIN_NAME(tx), _PIN_NAME(rx));

        // 有可能是打开串口完成以后跳回来
        if(Opened) return true;
    }
#endif

	USART_InitTypeDef  p;

	//串口引脚初始化
    _tx = new AlternatePort(tx);
#if defined(STM32F0) || defined(STM32F4)
    _rx = new AlternatePort(rx);
#else
    _rx = new InputPort(rx);
#endif

	// 不要关调试口，否则杯具
    if(_index != Sys.MessagePort) USART_DeInit(_port);
	// USART_DeInit其实就是关闭时钟，这里有点多此一举。但为了安全起见，还是使用

	// 检查重映射
#ifdef STM32F1XX
	if(IsRemap)
	{
		switch (_index) {
		case 0: AFIO->MAPR |= AFIO_MAPR_USART1_REMAP; break;
		case 1: AFIO->MAPR |= AFIO_MAPR_USART2_REMAP; break;
		case 2: AFIO->MAPR |= AFIO_MAPR_USART3_REMAP_FULLREMAP; break;
		}
	}
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE );
#endif

    // 打开 UART 时钟。必须先打开串口时钟，才配置引脚
#ifdef STM32F0XX
	switch(_index)
	{
		case COM1:	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);	break;//开启时钟
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

#ifdef STM32F0
    GPIO_PinAFConfig(_GROUP(tx), _PIN(tx), GPIO_AF_1);//将IO口映射为USART接口
    GPIO_PinAFConfig(_GROUP(rx), _PIN(rx), GPIO_AF_1);
#elif defined(STM32F4)
	const byte afs[] = { GPIO_AF_USART1, GPIO_AF_USART2, GPIO_AF_USART3, GPIO_AF_UART4, GPIO_AF_UART5, GPIO_AF_USART6, GPIO_AF_UART7, GPIO_AF_UART8 };
    GPIO_PinAFConfig(_GROUP(tx), _PIN(tx), afs[_index]);
    GPIO_PinAFConfig(_GROUP(rx), _PIN(rx), afs[_index]);
#endif

    USART_StructInit(&p);
	p.USART_BaudRate = _baudRate;
	p.USART_WordLength = _dataBits;
	p.USART_StopBits = _stopBits;
	p.USART_Parity = _parity;
	USART_Init(_port, &p);

	USART_ITConfig(_port, USART_IT_RXNE, ENABLE); // 串口接收中断配置
	// 初始化的时候会关闭所有中断，这里不需要单独关闭发送中断
	//USART_ITConfig(_port, USART_IT_TXE, DISABLE); // 不需要发送中断

	USART_Cmd(_port, ENABLE);//使能串口

	if(RS485) *RS485 = false;

    //Opened = true;

#if COM_DEBUG
    if(_index == Sys.MessagePort)
	{
		// 提前设置为已打开端口，ShowLog里面需要判断
		Opened = true;
		goto ShowLog;
	}
#endif

	return true;
}

// 关闭端口
void SerialPort::OnClose()
{
    debug_printf("~Serial%d Close\r\n", _index + 1);

    Pin tx, rx;
    GetPins(&tx, &rx);

    USART_DeInit(_port);

    if(_tx) delete _tx;
	if(_rx) delete _rx;
	_tx = NULL;
	_rx = NULL;

	// 检查重映射
#ifdef STM32F1XX
	if(IsRemap)
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
void TUsart_SendData(USART_TypeDef* port, const byte* data, uint times = 3000)
{
    while(USART_GetFlagStatus(port, USART_FLAG_TXE) == RESET && --times > 0);//等待发送完毕
    if(times > 0) USART_SendData(port, (ushort)*data);
}

// 向某个端口写入数据。如果size为0，则把data当作字符串，一直发送直到遇到\0为止
bool SerialPort::OnWrite(const byte* buf, uint size)
{
	if(RS485) *RS485 = true;

    if(size > 0)
    {
        for(int i=0; i<size; i++) TUsart_SendData(_port, buf++);
    }
    else
    {
        while(*buf) TUsart_SendData(_port, buf++);
    }

	if(RS485) *RS485 = false;

	return true;
}

// 从某个端口读取数据
uint SerialPort::OnRead(byte* buf, uint size)
{
	// 在100ms内接收数据
	uint msTimeout = 1;
	ulong us = Time.Current() + msTimeout * 1000;
	uint count = 0; // 收到的字节数
	while(count < size && Time.Current() < us)
	{
		// 轮询接收寄存器，收到数据则放入缓冲区
		if(USART_GetFlagStatus(_port, USART_FLAG_RXNE) != RESET)
		{
			*buf++ = (byte)USART_ReceiveData(_port);
			count++;
			us = Time.Current() + msTimeout * 1000;
		}
	}
	return count;
}

// 刷出某个端口中的数据
bool SerialPort::Flush(uint times)
{
	//uint times = 3000;
    while(USART_GetFlagStatus(_port, USART_FLAG_TXE) == RESET && --times > 0);//等待发送完毕
	return times > 0;
}

void SerialPort::Register(TransportHandler handler, void* param)
{
	ITransport::Register(handler, param);

	const byte irqs[] = UART_IRQs;
	byte irq = irqs[_index];
    if(handler)
	{
        Interrupt.SetPriority(irq, 1);

		Interrupt.Activate(irq, OnUsartReceive, this);
	}
    else
	{
		Interrupt.Deactivate(irq);
	}
}

// 真正的串口中断函数
void SerialPort::OnUsartReceive(ushort num, void* param)
{
	SerialPort* sp = (SerialPort*)param;
	if(sp && sp->HasHandler())
	{
		if(USART_GetITStatus(sp->_port, USART_IT_RXNE) != RESET)
		{
			// 从栈分配，节省内存
			byte buf[64];
			uint len = sp->Read(buf, ArrayLength(buf));
			if(len)
			{
				len = sp->OnReceive(buf, len);
				assert_param(len <= ArrayLength(buf));
				// 如果有数据，则反馈回去
				if(len) sp->Write(buf, len);
			}
		}
	}
}

// 获取引脚
void SerialPort::GetPins(Pin* txPin, Pin* rxPin)
{
    *rxPin = *txPin = P0;

	const Pin g_Uart_Pins[] = UART_PINS;
	const Pin g_Uart_Pins_Map[] = UART_PINS_FULLREMAP;
	const Pin* p = g_Uart_Pins;
	if(IsRemap) p = g_Uart_Pins_Map;

	int n = _index << 2;
	*txPin  = p[n];
	*rxPin  = p[n + 1];
}

extern "C"
{
    SerialPort* _printf_sp;
	bool isInFPutc;

    /* 重载fputc可以让用户程序使用printf函数 */
    int fputc(int ch, FILE *f)
    {
        if(!Sys.Inited) return ch;

        int _index = Sys.MessagePort;
        if(_index == COM_NONE) return ch;

		USART_TypeDef* g_Uart_Ports[] = UARTS;
        USART_TypeDef* port = g_Uart_Ports[_index];

		if(isInFPutc) return ch;
		isInFPutc = true;
        // 检查并打开串口
        if((port->CR1 & USART_CR1_UE) != USART_CR1_UE && _printf_sp == NULL)
        {
            //if(_printf_sp != NULL) delete _printf_sp;

            _printf_sp = new SerialPort(port);
            _printf_sp->Open();
        }

        TUsart_SendData(port, (byte*)&ch);

		isInFPutc = false;
        return ch;
    }
}

SerialPort* SerialPort::GetMessagePort()
{
	if(!_printf_sp)
	{
        int _index = Sys.MessagePort;
        if(_index == COM_NONE) return NULL;

		USART_TypeDef* g_Uart_Ports[] = UARTS;
        USART_TypeDef* port = g_Uart_Ports[_index];
		_printf_sp = new SerialPort(port);
		_printf_sp->Open();
	}
	return _printf_sp;
}
