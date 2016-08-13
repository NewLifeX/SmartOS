#ifndef __Proxy_H__
#define __Proxy_H__

#include "Sys.h"
#include "SerialPort.h"
#include "Message/BinaryPair.h"

class Proxy
{
public:
	String Name;			// 端口名
	MemoryStream* Cache;	// 缓存空间
	int		CacheSize;		// 缓存大小
	bool	Stamp;			// 时间戳开关
	int		TimeStamp;		// 时间戳

	Proxy();
	bool Open();
	bool Close();
	virtual bool OnOpen() = 0;
	virtual bool OnClose() = 0;

	virtual bool SetConfig(Dictionary<cstring, int>& config, String& str) = 0;
	virtual bool GetConfig(Dictionary<cstring, int>& config) = 0;
	virtual int	 Write(Buffer& data) = 0;
	virtual int  Read(Buffer& data, Buffer& input) = 0;
	bool Upload(Buffer& data);
};

class ComProxy : public Proxy
{
public:
	ComProxy(COM con);

	SerialPort port;

	byte	parity;
	byte	dataBits;
	byte	stopBits;
	int		baudRate;

	virtual bool OnOpen() override;

	virtual bool OnClose() override;

	virtual bool SetConfig(Dictionary<cstring, int>& config, String& str) override;

	virtual bool GetConfig(Dictionary<cstring, int>& config) override;

	virtual int Write(Buffer& data) override;

	virtual int Read(Buffer& data, Buffer& input) override;

private:

};

#endif
