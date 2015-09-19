#ifndef __CommandPort_H__
#define __CommandPort_H__

#include "Sys.h"
#include "Port.h"

// 命令端口。通过命令来控制输出口开关、取反、延时打开、打开延迟等高级功能
class CommandPort
{
private:
	uint	_tid;

public:
	OutputPort*	Port;	// 输出口
	byte*		Data;	// 绑定的数据位
	byte		Next;	// 定时执行的下一条命令
#if DEBUG
	String		Name;	// 名称
#endif

	CommandPort(OutputPort* port, byte* data = NULL);
	~CommandPort();

	void Set(byte cmd);	// 根据命令执行动作
	void Set();			// 根据绑定的数据位执行动作
	
	void StartAsync(int second);
};

#endif
