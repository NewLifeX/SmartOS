#ifndef __SerialPort_H__
#define __SerialPort_H__

#include "Core\Queue.h"
#include "Port.h"
#include "Power.h"
#include "Net\ITransport.h"

#define SERIAL_BAUDRATE 1024000

// 串口类
class SerialPort : public ITransport, public Power
{
private:
	friend class ComProxy;
	int		_baudRate;
	ushort	_dataBits;
	ushort	_parity;
	ushort	_stopBits;

	void Init();

public:
	char 		Name[5];	// 名称。COMxx，后面1字节\0表示结束
    byte		Remap;		// 重映射组
	OutputPort* RS485;		// RS485使能引脚
	int 		Error;		// 错误计数
	int			ByteTime;	// 字节间隔，最小1ms
	Pin			Pins[2];	// Tx/Rx
	Port*		Ports[2];	// Tx/Rx
	COM			Index;

    void*	State;

	// 收发缓冲区
	Queue	Tx;
	Queue	Rx;

	SerialPort();
    SerialPort(COM index, int baudRate = SERIAL_BAUDRATE);

	// 析构时自动关闭
    virtual ~SerialPort();

    void Set(COM index, int baudRate = SERIAL_BAUDRATE);
    void Set(byte dataBits, byte parity, byte stopBits);

	uint SendData(byte data, uint times = 3000);

    bool Flush(uint times = 3000);

	void SetBaudRate(int baudRate = SERIAL_BAUDRATE);

    virtual void Register(TransportHandler handler, void* param = nullptr);

	// 电源等级变更（如进入低功耗模式）时调用
	virtual void ChangePower(int level);

	virtual String& ToStr(String& str) const { return str + Name; }

	void OnTxHandler();
	void OnRxHandler();

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
	void Set485(bool flag);

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
