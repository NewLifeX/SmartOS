#ifndef __SerialPort_H__
#define __SerialPort_H__

#include "System.h"

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
	
	bool _opened;
    USART_TypeDef* _port;

public:
    SerialPort(int com, 
        int baudRate = 115200, 
        int parity = USART_Parity_No,       //无奇偶校验
        int dataBits = USART_WordLength_8b, //8位数据长度
        int stopBits = USART_StopBits_1);    //1位停止位
	// 析构时自动关闭
    ~SerialPort();

	bool Open();
    void Close();

    void Write(const string data, int size);
    int  Read(string data, uint size);
    void Flush();

    void Register(SerialPortReadHandler handler);
	void SetRemap();
    void GetPins(Pin* txPin, Pin* rxPin);
};

#endif
