#ifndef __SerialPort_H__
#define __SerialPort_H__

#include "Sys.h"
#include "Port.h"
#include "Net\ITransport.h"

// 串口类
class SerialPort : public ITransport
{
private:
	int _index;
	int _baudRate;
	int _parity;
	int _dataBits;
	int _stopBits;
	
    USART_TypeDef* _port;
	AlternatePort* _tx;
	InputPort* _rx;

    void Init(byte index, 
        int baudRate = 115200, 
        int parity = USART_Parity_No,       //无奇偶校验
        int dataBits = USART_WordLength_8b, //8位数据长度
        int stopBits = USART_StopBits_1);    //1位停止位

public:
	OutputPort* RS485;	// RS485使能引脚

    SerialPort(COM_Def index, 
        int baudRate = 115200, 
        int parity = USART_Parity_No,       //无奇偶校验
        int dataBits = USART_WordLength_8b, //8位数据长度
        int stopBits = USART_StopBits_1)    //1位停止位
	{
		Init(index, baudRate, parity, dataBits, stopBits);
	}

    SerialPort(USART_TypeDef* com, 
        int baudRate = 115200, 
        int parity = USART_Parity_No,       //无奇偶校验
        int dataBits = USART_WordLength_8b, //8位数据长度
        int stopBits = USART_StopBits_1);    //1位停止位
	// 析构时自动关闭
    virtual ~SerialPort();

    bool IsRemap;   // 是否重映射

    bool Flush(uint times = 3000);

    void GetPins(Pin* txPin, Pin* rxPin);

    virtual void Register(TransportHandler handler, void* param = NULL);

protected:
	virtual bool OnOpen();
    virtual void OnClose();

    virtual bool OnWrite(const byte* buf, uint size);
	virtual uint OnRead(byte* buf, uint size);

private:
	static void OnUsartReceive(ushort num, void* param);
};

#endif
