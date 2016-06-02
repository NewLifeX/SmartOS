#ifndef _UBlox_H_
#define _UBlox_H_

#include "Net\ITransport.h"
#include "Message\DataStore.h"

#include "BufferPort.h"

// 煮面的GPS传感器UBLOX
class UBlox : public BufferPort
{
public:
	cstring Header;	// 识别为数据包开头的字符串
	
	UBlox();

	void SetBaudRate(int baudRate);
	void SetRate();
	
	void EnterConfig();
	void SaveConfig();

protected:
	virtual bool OnOpen(bool isNew);
	virtual void OnReceive(const Buffer& bs, void* param);
};

#endif
