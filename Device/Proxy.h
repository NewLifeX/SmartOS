#ifndef __Proxy_H__
#define __Proxy_H__

#include "Sys.h"
#include "SerialPort.h"
#include "ProxyConfig.h"

class Proxy
{
public:
	cstring Name;			// 端口名
	MemoryStream* Cache;	// 缓存空间
	uint	UploadTaskId;	// 上传任务的ID
	uint	AutoTaskId;		// 自动任务ID，可以是定时Write数据
	int		TimeStamp;		// 时间戳

	int		CacheSize;		// 缓存数据包个数
	int		BufferSize;		// 缓冲区大小
	bool	EnableStamp;	// 时间戳开关
	bool	AutoStart;		// 自动启动

	Proxy();
	bool Open();
	bool Close();

	bool SetConfig(Dictionary<cstring, int>& config, String& str);
	bool GetConfig(Dictionary<char *, int>& config);
	virtual int	 Write(Buffer& data) = 0;
	virtual int  Read(Buffer& data, Buffer& input) = 0;
	void UploadTask();
	bool Upload(Buffer& data);

	void AutoTask();				// 自动处理任务
	bool LoadConfig();				// 从配置区内拿数据
	void SaveConfig();				// 保存配置信息

private:
	virtual bool OnOpen() = 0;
	virtual bool OnClose() = 0;
	virtual bool OnAutoTask() { return true; };	

	virtual bool OnSetConfig(Dictionary<cstring, int>& config, String& str) = 0;
	virtual bool OnGetConfig(Dictionary<char *, int>& config) = 0;

	virtual bool OnGetConfig(Stream& cfg) { return true; };
	virtual bool OnSetConfig(Stream& cfg) { return true; };
};

class ComProxy : public Proxy
{
public:
	ComProxy(COM con);

	SerialPort port;

	ushort	parity;
	ushort	dataBits;
	ushort	stopBits;
	int		baudRate;

	virtual bool OnSetConfig(Dictionary<cstring, int>& config, String& str) override;
	virtual bool OnGetConfig(Dictionary<char*, int>& config) override;

	virtual int Write(Buffer& data) override;
	virtual int Read(Buffer& data, Buffer& input) override;

private:
	virtual bool OnOpen() override;
	virtual bool OnClose() override;

	virtual bool OnGetConfig(Stream& cfg);
	virtual bool OnSetConfig(Stream& cfg);

	static uint Dispatch(ITransport* port, Buffer& bs, void* param, void* param2);
};

#endif
