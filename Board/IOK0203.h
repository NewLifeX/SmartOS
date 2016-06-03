#ifndef _IOK0203_H_
#define _IOK0203_H_

#include "Sys.h"
#include "Net\ITransport.h"

#include "TokenNet\TokenClient.h"

// 阿波罗0801/0802
class IOK0203
{
public:
	ISocketHost*	Host;			// 网络主机
	
	IOK0203();

	void Setup(ushort code, cstring name, COM message = COM1, int baudRate = 0);

	ITransport* Create2401();
	ISocketHost* Create8266(Action onNetReady = nullptr);

	TokenClient* CreateClient();

	void InitDHCP(Action onNetReady = nullptr);
};

#endif
