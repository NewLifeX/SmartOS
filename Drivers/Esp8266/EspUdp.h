#ifndef __EspUdp_H__
#define __EspUdp_H__

#include "EspSocket.h"

class EspUdp : public EspSocket
{
public:
	EspUdp(Esp8266& host, byte idx);

	virtual bool SendTo(const Buffer& bs, const IPEndPoint& remote);

	virtual String& ToStr(String& str) const { return str + "Udp_" + Local.Port; }

private:
	virtual bool OnWriteEx(const Buffer& bs, const void* opt);
};

#endif
