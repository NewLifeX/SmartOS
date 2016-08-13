#ifndef __ProxyFactory_H__
#define __ProxyFactory_H__

#include "Sys.h"
#include "TokenNet\TokenMessage.h"
#include "Message\BinaryPair.h"
#include "TokenNet\TokenClient.h"
#include "Device\Proxy.h"

class ProxyFactory
{
public:
	
	Dictionary<cstring, Proxy*> Proxys;
	TokenClient* Client;
	
	
	ProxyFactory();
	bool Open(TokenClient* client);

	// 配置
	bool GetConfig(const BinaryPair& args, Stream& result);
	bool SetConfig(const BinaryPair& args, Stream& result);

	// 获取设备列表
	bool QueryPorts(const BinaryPair& args, Stream& result);

	// 设备注册
	bool Register(cstring& name, Proxy* dev);

private:
	bool GetDic(String& str, Dictionary<cstring, char*>& dic);

public:
	static ProxyFactory* Current;
	static ProxyFactory* Create();
};

#endif
