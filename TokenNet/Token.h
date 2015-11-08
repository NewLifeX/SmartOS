#ifndef _Token_H_
#define _Token_H_

#include "Sys.h"
#include "Net\ITransport.h"
#include "Message\DataStore.h"

#include "TokenNet\TokenClient.h"
#include "TinyNet\TinyServer.h"

#include "TokenNet\Gateway.h"

#include "App\FlushPort.h"

// 令牌网
class Token
{
public:
	static void Setup(ushort code, const char* name, COM_Def message = COM1, int baudRate = 1024000);

	static ISocketHost* CreateW5500(SPI_TypeDef* spi, Pin irq, Pin rst = P0, Pin power = P0, IDataPort* led = NULL);

	static ITransport* Create2401(SPI_TypeDef* spi_, Pin ce, Pin irq, Pin power = P0, bool powerInvert = false);
	static ITransport* CreateShunCom(COM_Def index, int baudRate, Pin rst, Pin power, Pin slp, Pin cfg);
	
	static TokenClient* CreateClient(ISocketHost* host);
	static TinyServer* CreateServer(ITransport* port);
};

void* InitConfig(void* data, uint size);
void ClearConfig();

void CheckUserPress(InputPort* port, bool down, void* param = NULL);

#endif
