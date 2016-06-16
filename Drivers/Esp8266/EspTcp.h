#ifndef __EspTcp_H__
#define __EspTcp_H__

#include "EspSocket.h"

class EspTcp : public EspSocket
{
public:
	EspTcp(Esp8266& host, byte idx);

	virtual String& ToStr(String& str) const { return str + "Tcp_" + Local.Port; }
};

#endif
