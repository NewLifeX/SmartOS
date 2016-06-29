#ifndef _IOK027X_H_
#define _IOK027X_H_

#include "Sys.h"
#include "Net\ITransport.h"

#include "TokenNet\TokenClient.h"

// WIFI触摸开关 123位
class IOK027X
{
public:
	ISocketHost*	HostAP;			// 网络主机
	TokenClient*	Client;			// 
	
	IOK027X();

	void Setup(ushort code, cstring name, COM message = COM1, int baudRate = 0);

	void InitClient();
	void InitNet();

	ISocketHost* Create8266(bool apOnly);
	void* InitData(void* data, int size);
	void Register(int index, IDataPort& dp);

private:
	void *	Data;
	int		Size;
	void OpenClient(ISocketHost& host);
	ISocket* AddControl(ISocketHost& host, const NetUri& uri);
};

#endif
