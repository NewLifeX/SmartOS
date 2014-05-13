#include "System.h"
#include "Pin_STM32F0.h"

static USART_TypeDef* g_Uart_Ports[] = UARTS; 
static ushort g_Uart_Pins[] = UART_PINS;

bool TUsart_Open2(int com, int baudRate, int parity, int dataBits, int stopBits, int flowValue)
{
	USART_InitTypeDef  p;
    USART_TypeDef* port = g_Uart_Ports[com];

	//串口引脚初始化
    Sys.IO.OpenPort(g_Uart_Pins[(com<<2) + 0], GPIO_Mode_AF, GPIO_Speed_50MHz, GPIO_OType_PP);
    Sys.IO.OpenPort(g_Uart_Pins[(com<<2) + 1], GPIO_Mode_AF, GPIO_Speed_50MHz, GPIO_OType_PP);

	//RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);//开启时钟
    // 打开 UART 时钟
    if (com) { // COM2-5 on APB1
        RCC->APB1ENR |= RCC_APB1ENR_USART2EN >> 1 << com;
    } else { // COM1 on APB2
        RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    }

	p.USART_BaudRate = baudRate;
	p.USART_WordLength = dataBits;
	p.USART_StopBits = stopBits;
	p.USART_Parity = parity;
	p.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	p.USART_HardwareFlowControl = flowValue;
	USART_Init(port, &p);

	USART_ITConfig(port, USART_IT_RXNE, ENABLE);//串口接收中断配置

	USART_Cmd(port, ENABLE);//使能串口
    
    return true;
}

bool TUsart_Open(int com, int baudRate)
{
    return TUsart_Open2(com, baudRate, 
        USART_Parity_No,        //无奇偶校验
        USART_WordLength_8b,    //8位数据长度
        USART_StopBits_1,       //1位停止位
        USART_HardwareFlowControl_None//不使用硬件流控制
    );
}

void TUsart_Close(int com)
{
    USART_TypeDef* port = g_Uart_Ports[com];
    USART_DeInit(port);
    
    Sys.IO.Close(g_Uart_Pins[(com<<2) + 0]);
    Sys.IO.Close(g_Uart_Pins[(com<<2) + 1]);
}

void TUsart_SendData(USART_TypeDef* port, char* data)
{
    //while(!((port->ISR)&(1<<6)));//等待缓冲为空
    //port->TDR = *byte;//发送数据	
    USART_SendData(port, (ushort)*data);
    while(USART_GetFlagStatus(port, USART_FLAG_TXE) == RESET);//等待发送完毕
}

void TUsart_Write(int com, const string data, int size)
{
    int i;
    string byte = data;
    USART_TypeDef* port = g_Uart_Ports[com];
    
    if(size > 0)
    {
        for(i=0; i<size; i++) TUsart_SendData(port, byte++);
    }
    else
    {
        while(*byte) TUsart_SendData(port, byte++);
    }
}

int TUsart_Read(int com, string data, uint size)
{
    //USART_TypeDef* port = g_Uart_Ports[com];
    
    return 0;
}

void TUsart_Flush(int com)
{
    //USART_TypeDef* port = g_Uart_Ports[com];
}

void TUsart_Init(TUsart* this)
{
    this->Open  = TUsart_Open;
    this->Open2 = TUsart_Open2;
    this->Close = TUsart_Close;
    this->Write = TUsart_Write;
    this->Read  = TUsart_Read;
    this->Flush = TUsart_Flush;
}
