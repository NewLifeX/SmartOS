#ifndef __SerialPort_H__
#define __SerialPort_H__

#include "Sys.h"
#include "Port.h"
#include "Net\ITransport.h"

// 串口类
class SerialPort : public ITransport
{
private:
	int _com;
	int _baudRate;
	int _parity;
	int _dataBits;
	int _stopBits;
	
    USART_TypeDef* _port;
	AlternatePort* _tx;
	InputPort* _rx;

public:
	OutputPort* RS485;	// RS485使能引脚

    SerialPort(int com, 
        int baudRate = 115200, 
        int parity = USART_Parity_No,       //无奇偶校验
        int dataBits = USART_WordLength_8b, //8位数据长度
        int stopBits = USART_StopBits_1);    //1位停止位
	// 析构时自动关闭
    virtual ~SerialPort();

    //bool Opened;    // 是否打开
    bool IsRemap;   // 是否重映射

	//void Open();
    //void Close();

    //void Write(byte* buf, uint size);
    //void Write(const string data, uint size = 0) { ITransport::Write((byte*)data, size); }
    //uint  Read(byte* buf, uint size, uint msTimeout = 100);
    bool Flush(uint times = 3000);

    void GetPins(Pin* txPin, Pin* rxPin);

	// 数据接收委托，一般param用作目标对象
	//typedef void (*DataReceived)(SerialPort* sp, void* param);
    virtual void Register(TransportHandler handler, void* param = NULL);

protected:
	virtual bool OnOpen();
    virtual void OnClose();

    virtual void OnWrite(const byte* buf, uint size);
	virtual uint OnRead(byte* buf, uint size);

private:
	static void OnUsartReceive(ushort num, void* param);
	//DataReceived _Received;
	//void* _Param;
};

#endif
