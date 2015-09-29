#ifndef __FlushPort_H__
#define __FlushPort_H__

#include "Sys.h"
#include "Port.h"
#include "Message\DataStore.h"

// 闪烁端口
// 默认慢闪，Start后快闪一定时间，-1时间表示一直快闪，Stop停止
// 支持数据操作指令，参数为快闪毫秒数
class FlushPort : public IDataPort
{
private:
	uint	_tid;

public:
	OutputPort*	Port;

	int		Fast;	// 快闪间隔，默认50000微秒
	int		Slow;	// 慢闪间隔，默认1000000微秒
	int		Count;	// 剩余快闪次数

	FlushPort();
	~FlushPort();

	void Start(int ms = 1000);
	void Stop();
	void Flush();

	virtual int Write(byte* data);
};

#endif
