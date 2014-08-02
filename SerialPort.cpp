#include "Sys.h"
#include <stdio.h>

#include "Port.h"
#include "SerialPort.h"

// 默认波特率
#define USART_DEFAULT_BAUDRATE 115200

static USART_TypeDef* g_Uart_Ports[] = UARTS;
static const Pin g_Uart_Pins[] = UART_PINS;
static const Pin g_Uart_Pins_Map[] = UART_PINS_FULLREMAP;

static int SERIALPORT_IRQns[] = {
    USART1_IRQn, USART2_IRQn, 
#ifdef STM32F10X
    USART3_IRQn, UART4_IRQn, UART5_IRQn
#endif
};

SerialPort::SerialPort(int com, int baudRate, int parity, int dataBits, int stopBits)
{
    _com = com;
    _baudRate = baudRate;
    _parity = parity;
    _dataBits = dataBits;
    _stopBits = stopBits;

    _port = g_Uart_Ports[com];
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

	USART_InitTypeDef  p;
    Pin rx;
	Pin	tx;

    GetPins(&tx, &rx);

    USART_DeInit(_port);

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

	//串口引脚初始化
    _tx = new AlternatePort(tx, false, 10);
    _rx = new InputPort(rx);
	_tx->Restore = true;
	_rx->Restore = true;

#ifdef STM32F0XX
    GPIO_PinAFConfig(_GROUP(tx), _PIN(tx), GPIO_AF_1);//将IO口映射为USART接口
    GPIO_PinAFConfig(_GROUP(rx), _PIN(rx), GPIO_AF_1);
#endif

    USART_StructInit(&p);
	p.USART_BaudRate = _baudRate;
	p.USART_WordLength = _dataBits;
	p.USART_StopBits = _stopBits;
	p.USART_Parity = _parity;
	USART_Init(_port, &p);

	USART_ITConfig(_port, USART_IT_RXNE, ENABLE);//串口接收中断配置

    Interrupt.SetPriority(SERIALPORT_IRQns[_com], 1);

	USART_Cmd(_port, ENABLE);//使能串口

	if(RS485) *RS485 = false;

    Opened = true;
}

// 关闭端口
void SerialPort::Close()
{
    if(!Opened) return;

    // 清空接收委托
    Register(0);

    Pin tx, rx;
    GetPins(&tx, &rx);

    USART_DeInit(_port);

    if(_tx) delete _tx;
	if(_rx) delete _rx;

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
void TUsart_SendData(USART_TypeDef* port, char* data)
{
    while(USART_GetFlagStatus(port, USART_FLAG_TXE) == RESET);//等待发送完毕
    USART_SendData(port, (ushort)*data);
}

// 向某个端口写入数据。如果size为0，则把data当作字符串，一直发送直到遇到\0为止
void SerialPort::Write(const string data, int size)
{
    int i;
    string byte = data;

    Open();

	if(RS485) *RS485 = true;

    if(size > 0)
    {
        for(i=0; i<size; i++) TUsart_SendData(_port, byte++);
    }
    else
    {
        while(*byte) TUsart_SendData(_port, byte++);
    }

	if(RS485) *RS485 = false;
}

// 从某个端口读取数据
int SerialPort::Read(string data, uint size)
{
    Open();

    return 0;
}

// 刷出某个端口中的数据
void SerialPort::Flush()
{
    USART_TypeDef* port = g_Uart_Ports[_com];

    while(USART_GetFlagStatus(port, USART_FLAG_TXE) == RESET);//等待发送完毕
}

void SerialPort::Register(SerialPortReadHandler handler)
{
    if(handler)
	{
        _Received = handler;

		Interrupt.Activate(SERIALPORT_IRQns[_com], OnReceive, this);
	}
    else
	{
        _Received = 0;

		Interrupt.Deactivate(SERIALPORT_IRQns[_com]);
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
#ifdef STM32F10X
			byte data = (byte)sp->_port->DR;
#else
			byte data = (byte)sp->_port->RDR;
#endif
			sp->_Received(data);
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

    /* 重载fputc可以让用户程序使用printf函数 */
    int fputc(int ch, FILE *f)
    {
        if(!Sys.Inited) return ch;

        int _com = Sys.MessagePort;
        if(_com == COM_NONE) return ch;

        USART_TypeDef* port = g_Uart_Ports[_com];

        // 检查并打开串口
        if((port->CR1 & CR1_UE_Set) != CR1_UE_Set && _printf_sp == NULL)
        {
            if(_printf_sp != NULL) delete _printf_sp;

            _printf_sp = new SerialPort(_com);
            _printf_sp->Open();
        }

        TUsart_SendData(port, (char*)&ch);

        return ch;
    }
}

#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *   where the assert_param error has occurred.
  * @param file: pointer to the source file name
  * @param line: assert_param error line source number
  * @retval : None
  */
void assert_failed(uint8_t* file, uint32_t line)
{
    /* User can add his own implementation to report the file name and line number,
    ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    if(_printf_sp) printf("Assert Failed! Line %d, %s\r\n", line, file);

    /* Infinite loop */
    while (1) { }
}
#endif
