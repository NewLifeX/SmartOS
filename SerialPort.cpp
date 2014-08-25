#include "Sys.h"
#include <stdio.h>

#include "Port.h"
#include "SerialPort.h"

// 默认波特率
#define USART_DEFAULT_BAUDRATE 115200

static USART_TypeDef* g_Uart_Ports[] = UARTS;
static const Pin g_Uart_Pins[] = UART_PINS;
static const Pin g_Uart_Pins_Map[] = UART_PINS_FULLREMAP;

SerialPort::SerialPort(int com, int baudRate, int parity, int dataBits, int stopBits)
{
	assert_param(com >= 0 && com < ArrayLength(g_Uart_Ports));

    _com = com;
    _baudRate = baudRate;
    _parity = parity;
    _dataBits = dataBits;
    _stopBits = stopBits;
	_Received = NULL;	// 赋值NULL  以免野指针
    _port = g_Uart_Ports[com];

	_tx = NULL;
	_rx = NULL;
	RS485 = NULL;

	Opened = false;
	IsRemap = false;
}

// 析构时自动关闭
SerialPort::~SerialPort()
{
    Close();

	if(RS485) delete RS485;
	RS485 = NULL;
}

// 打开串口
void SerialPort::Open()
{
    if(Opened) return;

    Pin rx, tx;
    GetPins(&tx, &rx);

    //debug_printf("Serial%d Open(%d, %d, %d, %d)\r\n", _com + 1, _baudRate, _parity, _dataBits, _stopBits);
#if DEBUG
    if(_com != Sys.MessagePort)
    {
ShowLog:
        debug_printf("Serial%d Open(%d", _com + 1, _baudRate);
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
        if(Opened) return;
    }
#endif

	USART_InitTypeDef  p;

	//串口引脚初始化
    _tx = new AlternatePort(tx, false, 50);
    _rx = new InputPort(rx);
    //_tx->Write(true);

	// 不要关调试口，否则杯具
    if(_com != Sys.MessagePort) USART_DeInit(_port);

	// 检查重映射
#ifdef STM32F1XX
	if(IsRemap)
	{
		switch (_com) {
		case 0: AFIO->MAPR |= AFIO_MAPR_USART1_REMAP; break;
		case 1: AFIO->MAPR |= AFIO_MAPR_USART2_REMAP; break;
		case 2: AFIO->MAPR |= AFIO_MAPR_USART3_REMAP_FULLREMAP; break;
		}
	}
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE );
#endif

    // 打开 UART 时钟。必须先打开串口时钟，才配置引脚
#ifdef STM32F0XX
	switch(_com)
	{
		case COM1:	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);	break;//开启时钟
		case COM2:	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);	break;
		default:	break;
	}
#else
	if (_com) { // COM2-5 on APB1
        RCC->APB1ENR |= RCC_APB1ENR_USART2EN >> 1 << _com;
    } else { // COM1 on APB2
        RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    }
#endif

#ifdef STM32F0
    GPIO_PinAFConfig(_GROUP(tx), _PIN(tx), GPIO_AF_1);//将IO口映射为USART接口
    GPIO_PinAFConfig(_GROUP(rx), _PIN(rx), GPIO_AF_1);
#elif defined(STM32F4)
	byte afs[] = { GPIO_AF_USART1, GPIO_AF_USART2, GPIO_AF_USART3, GPIO_AF_UART4, GPIO_AF_UART5, GPIO_AF_USART6, GPIO_AF_UART7, GPIO_AF_UART8 };
    GPIO_PinAFConfig(_GROUP(tx), _PIN(tx), afs[_com]);
    GPIO_PinAFConfig(_GROUP(rx), _PIN(rx), afs[_com]);
#endif

    USART_StructInit(&p);
	p.USART_BaudRate = _baudRate;
	p.USART_WordLength = _dataBits;
	p.USART_StopBits = _stopBits;
	p.USART_Parity = _parity;
	USART_Init(_port, &p);

	USART_ITConfig(_port, USART_IT_RXNE, ENABLE);//串口接收中断配置

    //USART_ClearFlag(_port, USART_FLAG_TC);
    //USART_ClearFlag(_port, USART_FLAG_RXNE);
    //USART_ClearITPendingBit(_port, USART_IT_TC);
    //USART_ClearITPendingBit(_port, USART_IT_RXNE);
	USART_Cmd(_port, ENABLE);//使能串口

	if(RS485) *RS485 = false;

    Opened = true;

#if DEBUG
    if(_com == Sys.MessagePort) goto ShowLog;
#endif
}

// 关闭端口
void SerialPort::Close()
{
    if(!Opened) return;

    debug_printf("~Serial%d Close\r\n", _com + 1);

    // 清空接收委托
    Register(0);

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
		switch (_com) {
		case 0: AFIO->MAPR &= ~AFIO_MAPR_USART1_REMAP; break;
		case 1: AFIO->MAPR &= ~AFIO_MAPR_USART2_REMAP; break;
		case 2: AFIO->MAPR &= ~AFIO_MAPR_USART3_REMAP_FULLREMAP; break;
		}
	}
#endif

    Opened = false;
}

// 发送单一字节数据
void TUsart_SendData(USART_TypeDef* port, byte* data, uint times = 3000)
{
    while(USART_GetFlagStatus(port, USART_FLAG_TXE) == RESET && --times > 0);//等待发送完毕
    if(times > 0) USART_SendData(port, (ushort)*data);
}

// 向某个端口写入数据。如果size为0，则把data当作字符串，一直发送直到遇到\0为止
void SerialPort::Write(byte* buf, uint size)
{
    Open();

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
}

// 从某个端口读取数据
uint SerialPort::Read(byte* buf, uint size, uint msTimeout)
{
    Open();

	//return USART_ReceiveData(_port);

	// 在100ms内接收数据
	uint end = Time.NewTicks(msTimeout * 1000);
	uint count = 0; // 收到的字节数
	while(count < size && Time.CurrentTicks() < end)
	{
		// 轮询接收寄存器，收到数据则放入缓冲区
		if(USART_GetFlagStatus(_port, USART_FLAG_RXNE) != RESET)
		{
			*buf++ = (byte)USART_ReceiveData(_port);
			count++;
		}
	}
	return count;
}

// 刷出某个端口中的数据
void SerialPort::Flush()
{
	uint times = 300;
    while(USART_GetFlagStatus(_port, USART_FLAG_TXE) == RESET && --times > 0);//等待发送完毕
}

void SerialPort::Register(DataReceived handler, void* param)
{
    Open();

	_Received = handler;
	_Param = param;

	int SERIALPORT_IRQns[] = {
		USART1_IRQn, USART2_IRQn,
#ifdef STM32F10X
		USART3_IRQn, UART4_IRQn, UART5_IRQn
#endif
	};
    if(handler)
	{
        Interrupt.SetPriority(SERIALPORT_IRQns[_com], 1);

		Interrupt.Activate(SERIALPORT_IRQns[_com], OnReceive, this);
	}
    else
	{
		Interrupt.Deactivate(SERIALPORT_IRQns[_com]);
		_Received = NULL;
	}
}

// 真正的串口中断函数
void SerialPort::OnReceive(ushort num, void* param)
{
	SerialPort* sp = (SerialPort*)param;
	if(sp && sp->_Received)
	{
		if(USART_GetITStatus(sp->_port, USART_IT_RXNE) != RESET)
		{
			//uint count = sp->Read(sp->rx_buf, ArrayLength(sp->rx_buf));
			//if(count > 0) sp->_Received(sp, sp->rx_buf, count, sp->_Param);
			if(sp->_Received)			// 验证是否可用  以免造成不要的程序跑飞
				sp->_Received(sp, sp->_Param);
		}
	}
}

// 获取引脚
void SerialPort::GetPins(Pin* txPin, Pin* rxPin)
{
    *rxPin = *txPin = P0;

	const Pin* p = g_Uart_Pins;
	if(IsRemap) p = g_Uart_Pins_Map;

	int n = _com << 2;
	*txPin  = p[n];
	*rxPin  = p[n + 1];
}

extern "C"
{
    #define CR1_UE_Set                ((uint16_t)0x2000)  /*!< USART Enable Mask */

    SerialPort* _printf_sp;
	bool isInFPutc;

    /* 重载fputc可以让用户程序使用printf函数 */
    int fputc(int ch, FILE *f)
    {
        if(!Sys.Inited) return ch;

        int _com = Sys.MessagePort;
        if(_com == COM_NONE) return ch;

        USART_TypeDef* port = g_Uart_Ports[_com];

		if(isInFPutc) return ch;
		isInFPutc = true;
        // 检查并打开串口
        if((port->CR1 & CR1_UE_Set) != CR1_UE_Set && _printf_sp == NULL)
        {
            if(_printf_sp != NULL) delete _printf_sp;

            _printf_sp = new SerialPort(_com);
            _printf_sp->Open();
        }

        TUsart_SendData(port, (byte*)&ch);

		isInFPutc = false;
        return ch;
    }
}
