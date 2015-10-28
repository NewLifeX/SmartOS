#ifndef _Tiny_H_
#define _Tiny_H_

#include "Sys.h"
#include "Net\ITransport.h"

#include "TinyNet\TinyClient.h"

void Setup(ushort code, const char* name, COM_Def message = COM1, int baudRate = 1024000);

void* InitConfig(void* data, uint size);

ITransport* Create2401(SPI_TypeDef* spi_, Pin ce, Pin irq, Pin power = P0, bool powerInvert = false);
ITransport* CreateShunCom(COM_Def index, int baudRate, Pin rst, Pin power, Pin slp, Pin cfg);

TinyClient* CreateTinyClient(ITransport* port);

//void NoUsed();

#endif
