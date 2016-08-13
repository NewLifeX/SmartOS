#ifndef __Proxy_H__
#define __Proxy_H__

#include "Sys.h"
#include "SerialPort.h"
#include "Message/BinaryPair.h"

class Proxy
{
public:
	String name;			// 端口名
	MemoryStream* Cache;	// 缓存空间
	int		CacheSize;		// 缓存大小
	bool	Stamp;			// 时间戳开关
	int		TimeStamp;		// 时间戳

	Proxy();
	bool Open();
	bool Close();
	virtual bool OnOpen() = 0;
	virtual bool OnClose() = 0;

	virtual bool SetConfig(Dictionary<cstring, cstring>& config, String& str) = 0;
	virtual bool GetConfig(Dictionary<cstring, cstring>& config) = 0;
	virtual int	 Write(Buffer& data) = 0;
	virtual Buffer& Read(Buffer& data) = 0;
	bool Upload(Buffer& data);
};


#endif
