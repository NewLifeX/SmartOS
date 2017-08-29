#ifndef _LinkBoard_H_
#define _LinkBoard_H_

#include "Net\Socket.h"

#include "Device\Spi.h"
#include "Device\SerialPort.h"

#include "BaseBoard.h"

#include "Link\LinkClient.h"

// 物联协议板级包基类
class LinkBoard : public BaseBoard
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

	SpiConfig		Net;
	SerialConfig	Esp;

	// 打开以太网W5500
	NetworkInterface* Create5500();
	// 打开Esp8266，作为主控或者纯AP
	NetworkInterface* Create8266();

private:
	void*	Data;
	int		Size;
};

#endif
