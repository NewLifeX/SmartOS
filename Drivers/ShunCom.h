#ifndef __ShunCom_H__
#define __ShunCom_H__

#include "Sys.h"
#include "Port.h"
#include "Net\ITransport.h"
#include "Message\DataStore.h"

// 上海顺舟Zigbee协议
// 主站发送所有从站收到，从站发送只有主站收到
class ShunCom : public PackPort
{
private:

public:
	OutputPort	Reset;	// 复位
	IDataPort*	Led;	// 指示灯

	OutputPort	Power;	// 电源
	OutputPort	Sleep;	// 睡眠
	OutputPort	Config;	// 配置

	InputPort	Run;	// 组网成功后闪烁
	InputPort	Net;	// 组网中闪烁
	InputPort	Alarm;	// 警告错误

	ShunCom();

	void Init(ITransport* port, Pin rst = P0);

	void ShowConfig();

    virtual bool OnWrite(const ByteArray& bs);
	// 引发数据到达事件
	virtual uint OnReceive(ByteArray& bs, void* param);

	virtual string ToString() { return "ShunCom"; }

protected:
	virtual bool OnOpen();
    virtual void OnClose();
};

#endif
