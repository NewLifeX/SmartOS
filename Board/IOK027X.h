#ifndef _IOK027X_H_
#define _IOK027X_H_

#include "Sys.h"
#include "Net\ITransport.h"

#include "TokenNet\TokenClient.h"

// WIFI触摸开关 123位
class IOK027X
{
public:
	ISocketHost*	Host;			// 网络主机
	ISocketHost*	HostAP;			// 网络主机
	TokenClient*	Client;			// 
	
	IOK027X();

	void Setup(ushort code, cstring name, COM message = COM1, int baudRate = 0);

	static ISocketHost* Create8266();

	ISocketHost* Open8266(bool apOnly);
	
	void CreateClient();
	
	void OpenClient();
	void AddControl(ISocketHost& host, TokenConfig& cfg);
};

#endif
