#ifndef _LinkBoard_H_
#define _LinkBoard_H_

#include "Link\LinkClient.h"

// 物联协议板级包基类
class LinkBoard
{
public:
	LinkClient*	Client;	// 物联客户端

	LinkBoard();

	// 设置数据区
	void* InitData(void* data, int size);
	// 写入数据区并上报
	void Write(uint offset, byte data);

	typedef bool(*Handler)(uint offset, uint size, bool write);
	void Register(uint offset, uint size, Handler hook);
	void Register(uint offset, IDataPort& dp);

	void OnLongPress(InputPort* port, bool down);
	void Restore();

	void InitClient();

private:
	void*	Data;
	int		Size;
};

#endif
