
#ifndef __BLU40_H__
#define __BLU40_H__

#include "Sys.h"
#include "Port.h"
#include "Net\ITransport.h"
#include "SerialPort.h"

// 司卡乐 CC2540
class Blu40 : public ITransport
{
private:
	SerialPort *_port;
	OutputPort *_rts;
	InputPort *_cts;
	OutputPort *_rst;
	int _baudRate;
public:
	Blu40();
	Blu40(SerialPort *port,Pin rts = P0 ,Pin cts = P0, OutputPort * rst = NULL);
	virtual ~Blu40();
	void Init(SerialPort *port = NULL,Pin rts = P0,Pin cts = P0,OutputPort * rst = NULL);
	
	virtual void Register(TransportHandler handler, void* param = NULL);
	virtual void Reset(void);

	// 设置波特路
	bool SetBP(int BP);
	// 检查设置是否成功 使用大部分指令
	bool CheckSet();
	// 设置发送信号强度 DB数
	bool SetTPL(int TPLDB);
	// 设置蓝牙名称
	bool SetName(string name);
	// 设置产品识别码 硬件类型code
	bool SetPID(ushort pid);
	
	virtual string ToString() { return "Blutooth4.0"; }
protected:
	virtual bool OnOpen() { return _port->Open(); }
    virtual void OnClose() { _port->Close(); }

    virtual bool OnWrite(const byte* buf, uint len) { return _port->Write(buf, len); }
	virtual uint OnRead(byte* buf, uint len) { return _port->Read(buf, len); }

	static uint OnPortReceive(ITransport* sender, byte* buf, uint len, void* param);
};
#endif
