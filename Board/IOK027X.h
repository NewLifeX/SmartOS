#ifndef _IOK027X_H_
#define _IOK027X_H_

#include "Sys.h"
#include "Net\ITransport.h"

#include "TokenNet\TokenClient.h"

// WIFI触摸开关 123位
class IOK027X
{
public:
	List<Pin>	LedPins;
	List<OutputPort*>	Leds;

	ISocketHost*	Host;			// 网络主机
	TokenClient*	Client;			//

	IOK027X();

	void Init(ushort code, cstring name, COM message = COM1);

	void* InitData(void* data, int size);
	void Register(int index, IDataPort& dp);

	void InitLeds();

	ISocketHost* Create8266();

	void InitClient();
	void InitNet();

	void Restore();
	void OnLongPress(InputPort* port, bool down);

private:
	void*	Data;
	int		Size;

	void OpenClient(ISocketHost& host);
	TokenController* AddControl(ISocketHost& host, const NetUri& uri, ushort localPort);
};

#endif
