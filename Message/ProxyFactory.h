#ifndef __ProxyFactory_H__
#define __ProxyFactory_H__

#include "Sys.h"
#include "TokenNet\TokenMessage.h"
#include "Message\BinaryPair.h"
#include "TokenNet\TokenClient.h"
#include "Device\Proxy.h"
#include "Core\Dictionary.h"

class ProxyFactory
{
public:
	
	Dictionary<cstring, Proxy*> Proxys;
	TokenClient* Client;
	
	
	ProxyFactory();
	// 工程打开
	bool Open(TokenClient* client);
	// 端口注册
	bool Register(Proxy* dev);
	// 开关
	bool PortOpen(const BinaryPair& args, Stream& result);
	bool PortClose(const BinaryPair& args, Stream& result);
	// 读写
	bool Write(const BinaryPair& args, Stream& result);
	bool Read(const BinaryPair& args, Stream& result);
	// 配置
	bool GetConfig(const BinaryPair& args, Stream& result);
	bool SetConfig(const BinaryPair& args, Stream& result);
	// 获取设备列表
	bool QueryPorts(const BinaryPair& args, Stream& result);
	
	// XXX7，自动bool Proxy/SetAuto(bool auto, Byte[] boot)。		// 设备上电后自动发送，可能需要引导数据   => 合并到2
	// 8，查询状态Byte[] Proxy/Query(String port, String key)。		// 支持查询是否已打开、已缓存数据量等。

	bool Upload(Proxy& port,Buffer& data);
	bool UpOpen(Proxy& port);

	bool AutoStart();		// 自动启动

private:
	// 从字符串里拿出字典形式的配置信息
	bool GetDic(String& str, Dictionary<cstring, int>& dic);
	// 从数据包里解析出Proxy
	Proxy* GetPort(const BinaryPair& args);

public:
	static ProxyFactory* Current;
	static ProxyFactory* Create();
};

#endif
