#include "Pin_STM32F0.h"
#include <stdio.h>

#include "Port.h"
#include "SerialPort.h"

// 默认波特率
#define USART_DEFAULT_BAUDRATE 115200

static USART_TypeDef* g_Uart_Ports[] = UARTS; 
static const Pin g_Uart_Pins[] = UART_PINS;
static const Pin g_Uart_Pins_Map[] = UART_PINS_FULLREMAP;

#ifdef STM32F1XX
	// 串口接收委托
	static SerialPortReadHandler UsartHandlers[6] = {0, 0, 0, 0, 0, 0};
#else
	static SerialPortReadHandler UsartHandlers[2] = {0, 0};		//只有2个串口
#endif

SerialPort::SerialPort(int com, int baudRate, int parity, int dataBits, int stopBits)
{
    _com = com;
    _baudRate = baudRate;
    _parity = parity;
    _dataBits = dataBits;
    _stopBits = stopBits;
}

// 析构时自动关闭
SerialPort::~SerialPort()
{
    Close();
}

// 打开串口
void SerialPort::Open()
{
    if(Opened) return;

	USART_InitTypeDef  p;
    Pin rx;
	Pin	tx;
    NVIC_InitTypeDef NVIC_InitStructure;

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
#ifdef STM32F0XX
    //Sys.IO.OpenPort(tx, GPIO_Mode_AF, GPIO_Speed_10MHz, GPIO_OType_PP,GPIO_PuPd_NOPULL);
    //Sys.IO.OpenPort(rx, GPIO_Mode_AF, GPIO_Speed_10MHz, GPIO_OType_PP,GPIO_PuPd_NOPULL);
    Port::SetAlternate(tx, false, Port::Speed_10MHz);
    Port::SetInput(rx, true);	//TX		RX		COM		AF
	//PA2		PA3		COM2	AF1
	//PA9		PA10	COM1	AF1
	//PA14		PA15	COM2	AF1
	//PB6		PB7		COM1	AF0
    GPIO_PinAFConfig(_GROUP(tx), _PIN(tx), GPIO_AF_1);//将IO口映射为USART接口
    GPIO_PinAFConfig(_GROUP(rx), _PIN(rx), GPIO_AF_1);
#else
    //Sys.IO.OpenPort(tx, GPIO_Mode_AF_PP, GPIO_Speed_50MHz);
    //Sys.IO.Open(rx, GPIO_Mode_IN_FLOATING);
    Port::SetAlternate(tx, false, Port::Speed_50MHz);
    Port::SetInput(rx, true);
#endif

    USART_StructInit(&p);
	p.USART_BaudRate = _baudRate;
	p.USART_WordLength = _dataBits;
	p.USART_StopBits = _stopBits;
	p.USART_Parity = _parity;
	USART_Init(_port, &p);

	USART_ITConfig(_port, USART_IT_RXNE, ENABLE);//串口接收中断配置

    /* Configure the NVIC Preemption Priority Bits */  
//#ifdef STM32F10X				//全局设置  放到system.c里面
//    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
//#endif
	if(_com==COM1)
        NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn; 
	else
        NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn; 
		
#ifdef STM32F10X
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0xff;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0xff;
#else
    NVIC_InitStructure.NVIC_IRQChannelPriority = 0x01;
#endif
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;

    switch (_com) {
        case 0: NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn; break;
        case 1: NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn; break;
#if STM32F10x
//        case 1: NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn; break;
        case 2: NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn; break;
        case 3: NVIC_InitStructure.NVIC_IRQChannel = UART4_IRQn; break;
        case 4: NVIC_InitStructure.NVIC_IRQChannel = UART5_IRQn; break;
#endif
    }

    NVIC_Init(&NVIC_InitStructure);

	USART_Cmd(_port, ENABLE);//使能串口

    Opened = true;
}

// 关闭端口
void SerialPort::Close()
{
    if(!Opened) return;

    Pin tx, rx;
    GetPins(&tx, &rx);

    USART_DeInit(_port);
    
    //Sys.IO.Close(tx);
    //Sys.IO.Close(rx);
    Port::SetAlternate(tx);
    Port::SetAlternate(rx);

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

    // 清空接收委托
    Register(0);

    Opened = false;
}

// 发送单一字节数据
void TUsart_SendData(USART_TypeDef* port, char* data)
{
    //USART_SendData(port, (ushort)*data);
    while(USART_GetFlagStatus(port, USART_FLAG_TXE) == RESET);//等待发送完毕
    USART_SendData(port, (ushort)*data);
}

// 向某个端口写入数据。如果size为0，则把data当作字符串，一直发送直到遇到\0为止
void SerialPort::Write(const string data, int size)
{
    int i;
    string byte = data;

    Open();
    
    if(size > 0)
    {
        for(i=0; i<size; i++) TUsart_SendData(_port, byte++);
    }
    else
    {
        while(*byte) TUsart_SendData(_port, byte++);
    }
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
        UsartHandlers[_com] = handler;
    else
        UsartHandlers[_com] = 0;
}

// 真正的串口中断函数
void OnReceive(int _com)
{
    USART_TypeDef* port = g_Uart_Ports[_com];

    if(USART_GetITStatus(port, USART_IT_RXNE) != RESET)
    { 	
#ifdef STM32F10X
        if(UsartHandlers[_com]) UsartHandlers[_com]((byte)port->DR);
#else
        if(UsartHandlers[_com]) UsartHandlers[_com]((byte)port->RDR);
#endif
    } 
}

//所有中断重映射到onreceive函数
void USART1_IRQHandler(void) { OnReceive(0); }
void USART2_IRQHandler(void) { OnReceive(1); }
#ifdef STM32F10X
void USART3_IRQHandler(void) { OnReceive(2); }
void USART4_IRQHandler(void) { OnReceive(3); }
void USART5_IRQHandler(void) { OnReceive(4); }
#endif

// 获取引脚
void SerialPort::GetPins(Pin* txPin, Pin* rxPin)
{
	const Pin* p;

    *rxPin = *txPin = P0;
	
	p = g_Uart_Pins;
	if(IsRemap) p = g_Uart_Pins_Map;

	_com = _com << 2;
	*txPin  = p[_com];
	*rxPin  = p[_com + 1];
}

extern "C"
{
/* 重载fputc可以让用户程序使用printf函数 */
int fputc(int ch, FILE *f)
{
    int _com = Sys.MessagePort;
    if(_com == COM_NONE) return ch;

    USART_TypeDef* port = g_Uart_Ports[_com];

    //while(!((port->ISR)&(1<<6)));//等待缓冲为空
    //port->TDR = (byte) ch;
    //USART_SendData(port, (unsigned char) ch);

#ifdef STM32F0XX
    //while(!((port->ISR)&(1<<6)));//等待缓冲为空
#else
    //while (!(port->SR & USART_FLAG_TXE));
#endif
    //USART_SendData(port, (unsigned char) ch);
    TUsart_SendData(port, (char*)&ch);

    return ch;
}
}
