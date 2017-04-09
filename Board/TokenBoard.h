#ifndef _TokenBoard_H_
#define _TokenBoard_H_

#include "BaseBoard.h"

#include "TokenNet\TokenClient.h"

// 令牌协议板级包基类
class TokenBoard : public BaseBoard
{
public:
	TokenClient*	Client;	// 令牌客户端

	TokenBoard();

	// 设置数据区
	void* InitData(void* data, int size);
	// 写入数据区并上报
	void Write(uint offset, byte data);
	// 获取客户端的状态0，未握手，1已握手，2已经登陆
	//int GetStatus();

	typedef bool(*Handler)(uint offset, uint size, bool write);
	void Register(uint offset, uint size, Handler hook);
	void Register(uint offset, IDataPort& dp);

	// invoke指令
	void Invoke(const String& ation, const Buffer& bs);
	void OnLongPress(InputPort* port, bool down);
	void Restore();

	void InitClient();

private:
	void*	Data;
	int		Size;
};

#endif
