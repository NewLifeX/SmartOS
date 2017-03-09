#ifndef __A67_H__
#define __A67_H__

#include "Device\Port.h"

#include "App\AT.h"

#include "Net\Socket.h"
#include "Net\NetworkInterface.h"
#include "Net\ITransport.h"

#include "Message\DataStore.h"
#include "Message\Pair.h"

#include "GSM07.h"

// A6/A6C/A7
class A67 : public GSM07
{
public:
	/******************************** 扩展指令 ********************************/
	bool GetGPS();
	bool SetGPS(bool enable, int rate = 0);

	bool GetAGPS();
	bool SetAGPS(bool enable, int rate = 0);

	bool CameraStart(int mode);
	bool CameraStop();
	bool CameraCapture();
	String CameraRead(int from, int to = 0);
	// "192.168.1.111/A6C/123.jpg",80
	bool CameraPost(const String& url, int port = 80);

	String HttpGet(const String& url, int port = 80);
	bool HttpPost(const String& url, int port, const Buffer& data);
};

#endif
