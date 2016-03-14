#ifndef __BLU40_H__
#define __BLU40_H__

#include "Sys.h"
#include "Port.h"
#include "Net\ITransport.h"
#include "SerialPort.h"

// 思卡乐 CC2540
class Blu40 : public PackPort
{
private:
	SerialPort *_port;
	OutputPort *_rts;
//	InputPort *_cts;
	OutputPort *_rst;
	OutputPort *_sleep;	// 拉低时蓝牙工作，否则睡眠不工作
	int _baudRate;

public:
	Blu40();
	Blu40(SerialPort *port,Pin rts = P0 ,/*Pin cts = P0,*/Pin sleep=P0, OutputPort * rst = nullptr);
	virtual ~Blu40();
	void Init();
	void Init(SerialPort *port ,Pin rts = P0,/*Pin cts = P0,*/Pin sleep=P0, OutputPort * rst = nullptr);

	virtual void Register(TransportHandler handler, void* param = nullptr);
	virtual void Reset(void);

	// 设置波特路
	bool SetBP(int BP);
	// 检查设置是否成功 使用大部分指令
	bool CheckSet();
	// 设置发送信号强度 DB数
	bool SetTPL(int TPLDB);
	// 设置蓝牙名称
	bool SetName(const char* name);
	// 设置产品识别码 硬件类型code
	bool SetPID(ushort pid);

	//virtual const String ToString() const { return String("BLE4"); }

protected:
	virtual bool OnOpen();
	virtual void OnClose();
};
#endif
