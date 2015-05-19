
#ifndef __Zigbee_H__
#define __Zigbee_H__

#include "Sys.h"
#include "Port.h"
#include "Net\ITransport.h"

// 司卡乐 CC2540
class Blu40 : public ITransport
{
private:
	ITransport *_port;
	OutputPort *_rts;
	InputPort *_cts;
	OutputPort *_rst;
public:
	Blu40();
	Blu40(ITransport *port = NULL,Pin rts = P0 ,Pin cts = P0, OutputPort * rst = NULL);
	virtual ~Blu40();
	void Init(ITransport *port = NULL,Pin rts = P0,Pin cts = P0,OutputPort * rst = NULL);
	
	virtual void Register(TransportHandler handler, void* param = NULL);
	virtual void Reset(void);
	
protected:
	virtual bool OnOpen() { return _port->Open(); }
    virtual void OnClose() { _port->Close(); }

    virtual bool OnWrite(const byte* buf, uint len) { return _port->Write(buf, len); }
	virtual uint OnRead(byte* buf, uint len) { return _port->Read(buf, len); }

	static uint OnPortReceive(ITransport* sender, byte* buf, uint len, void* param);
	
	// 设置波特路
	bool SetBP(uint BP);
	// 检查设置是否成功 使用大部分指令
	bool CheckSet();
	// 设置发送信号强度 DB数
	const int TPLNum[] = {-23,-6,0,4};
	bool SetTPL(int TPLDB);
	// 设置蓝牙名称
	bool SetName(string name);
};