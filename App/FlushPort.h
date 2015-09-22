#ifndef __FlushPort_H__
#define __FlushPort_H__

#include "Sys.h"
#include "Port.h"
#include "Message\DataStore.h"

// 闪烁端口
class FlushPort : public IDataPort
{
private:
	uint	_tid;

public:
	OutputPort*	Port;

	int		Fast;	// 快闪间隔
	int		Slow;	// 慢闪间隔
	int		Count;	// 剩余快闪次数
	
	FlushPort();
	~FlushPort();

	void Start(int ms = 1000);
	void Flush();
	
	virtual int Write(byte* data);
};

#endif
