#ifndef _BufferPort_H_
#define _BufferPort_H_

#include "Net\ITransport.h"
#include "Message\DataStore.h"

// 缓冲区端口。目标设备定时发出数据，只需要保存最新一份副本
class BufferPort
{
public:
    const char*	Name;
	ITransport*	Port;
	int			Speed;
	COM			Com;
	bool		Opened;

	IDataPort*	Led;	// 指示灯

	ByteArray	Buf;	// 用于接收数据的缓冲区，外部在打开前设置大小

    BufferPort();
    ~BufferPort();

	bool Open();
	void Close();

protected:
	virtual bool OnOpen(bool isNew);
	virtual void OnReceive(const Buffer& bs, void* param);

private:
	static uint OnReceive(ITransport* transport, Buffer& bs, void* param, void* param2);
};

#endif
