#ifndef _Tiny_H_
#define _Tiny_H_

#include "Sys.h"
#include "Power.h"
#include "Net\ITransport.h"

#include "TinyNet\TinyClient.h"

#include "App\Button_GrayLevel.h"

void Setup(ushort code, const char* name, COM_Def message = COM1, int baudRate = 0);

void* InitConfig(void* data, uint size);
void ClearConfig();

ITransport* Create2401(byte spi, Pin ce, Pin irq, Pin power = P0, bool powerInvert = false, IDataPort* led = NULL);
ITransport* CreateShunCom(COM_Def index, int baudRate, Pin rst, Pin power, Pin slp, Pin cfg, IDataPort* led = NULL);

TinyClient* CreateTinyClient(ITransport* port);

bool CheckUserPress(InputPort* port, bool down, void* param = NULL);
void InitButtonPress(Button_GrayLevel* btns, byte count);

void SetPower(ITransport* port);

//void NoUsed();

#endif
