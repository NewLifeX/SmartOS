#ifndef __HMI_H__
#define __HMI_H__

#include "Device\SerialPort.h"

class HMI
{
public:
	ITransport*	Port;	// 传输口

	HMI();
	~HMI();

	// 初始化串口屏的串口
	void Init(COM idx, int baudrate = 115200); 
	// 初始传输口
	void Init(ITransport* port);	

	// 引发数据到达事件
	uint OnReceive(Buffer& bs, void* param);
	static uint OnPortReceive(ITransport* sender, Buffer& bs, void* param, void* param2);

	// 发送指令到串口屏
	void Send(const String& cmd);
	// 发送结束标志
	void SenDFlag();
};

#endif
