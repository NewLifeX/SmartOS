﻿#ifndef _UBlox_H_
#define _UBlox_H_

#include "Net\ITransport.h"
#include "Message\DataStore.h"

#include "BufferPort.h"

// 煮面的GPS传感器UBLOX
class UBlox : public BufferPort
{
public:
	bool SetBaudRate(int baudRate);

protected:
	virtual void OnReceive(const Array& bs, void* param);
};

#endif
