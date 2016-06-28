#include "EspTcp.h"

/******************************** Tcp ********************************/

EspTcp::EspTcp(Esp8266& host, byte idx)
	: EspSocket(host, NetType::Tcp, idx)
{

}
