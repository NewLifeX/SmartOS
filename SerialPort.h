#ifndef __SerialPort_H__
#define __SerialPort_H__

#include "Sys.h"

/* 串口定义 */
#define COM1 0
#define COM2 1
#define COM3 2
#define COM4 3
#define COM5 4
#define COM_NONE 0xFF

// 读取委托
typedef void (*SerialPortReadHandler)(byte data);

// 串口类
class SerialPort
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

	static void OnReceive(ushort num, void* param);
	SerialPortReadHandler _Received;

public:
	OutputPort* RS485;	// RS485使能引脚

    SerialPort(int com, 
        int baudRate = 115200, 
        int parity = USART_Parity_No,       //无奇偶校验
        int dataBits = USART_WordLength_8b, //8位数据长度
        int stopBits = USART_StopBits_1);    //1位停止位
	// 析构时自动关闭
    ~SerialPort();

    bool Opened;    // 是否打开
    bool IsRemap;   // 是否重映射

	void Open();
    void Close();

    void Write(const string data, int size);
    int  Read(string data, uint size);
    void Flush();

    void Register(SerialPortReadHandler handler);
    void GetPins(Pin* txPin, Pin* rxPin);
};

#endif
