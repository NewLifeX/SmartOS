#ifndef __SerialPort_H__
#define __SerialPort_H__

#include "Port.h"
#include "Queue.h"
#include "Power.h"
#include "Net\ITransport.h"
// #include "Proxy.h"

#define SERIAL_BAUDRATE 1024000

// 串口类
class SerialPort : public ITransport, public Power
{
private:
	friend class ComProxy;
	byte	_index;
	ushort	_parity;
	ushort	_dataBits;
	ushort	_stopBits;
	int		_baudRate;

    void*	_port;
	AlternatePort	_tx;
#if defined(STM32F0) || defined(GD32F150) || defined(STM32F4)
	AlternatePort	_rx;
#else
	InputPort	_rx;
#endif

	void Init();

public:
	char 		Name[5];	// 名称。COMxx，后面1字节\0表示结束
    bool		Remap;		// 是否重映射
	OutputPort* RS485;		// RS485使能引脚
	int 		Error;		// 错误计数
	int			ByteTime;	// 字节间隔，最小1ms

	// 收发缓冲区
	Queue	Tx;
	Queue	Rx;

	SerialPort();
    SerialPort(COM index, int baudRate = SERIAL_BAUDRATE);

	// 析构时自动关闭
    virtual ~SerialPort();

    void Set(COM index, int baudRate = SERIAL_BAUDRATE);
    void Set(byte parity, byte dataBits, byte stopBits);

	uint SendData(byte data, uint times = 3000);

    bool Flush(uint times = 3000);

	void SetBaudRate(int baudRate = SERIAL_BAUDRATE);

    void GetPins(Pin* txPin, Pin* rxPin);

    virtual void Register(TransportHandler handler, void* param = nullptr);

	// 电源等级变更（如进入低功耗模式）时调用
	virtual void ChangePower(int level);

	//virtual String ToString() const { return String(Name); }
	virtual String& ToStr(String& str) const { return str + Name; }

	static SerialPort* GetMessagePort();

#ifdef DEBUG
	static void Test();
#endif

protected:
	virtual bool OnOpen();
    virtual void OnClose();

    virtual bool OnWrite(const Buffer& bs);
	virtual uint OnRead(Buffer& bs);

private:
	static void OnHandler(ushort num, void* param);
	void OnTxHandler();
	void OnRxHandler();

	void*	_task;
	uint	_taskidRx;
	void ReceiveTask();

	void OnInit();
	bool OnSet();
	void OnOpen2();
	void OnClose2();
	void OnWrite2();
};

#endif
