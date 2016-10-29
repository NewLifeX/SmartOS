#ifndef __ProxyCfg_H__
#define __ProxyCfg_H__

#include "Sys.h"
#include "Config.h"

class ProxyConfig : public ConfigBase
{
public:
	int Length;
	byte AutoStart;			// 自动启动
	byte EnableStamp;		// 时间戳开关
	int		CacheSize;		// 缓存数据包个数
	int		BufferSize;		// 缓冲区大小

	byte PortCfg[64];		// 子类自定义配置
	// byte FixedCmd[128];		// 定时任务的命令+数据

	byte TagEnd;

	ProxyConfig(cstring name = nullptr);
	virtual void Init();
};


#endif
