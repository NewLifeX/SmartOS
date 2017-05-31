#ifndef __GatherConfig_H__
#define __GatherConfig_H__

#include "..\Config.h"
#include "Message\BinaryPair.h"
#include "Message\DataStore.h"

// 采集配置
class GatherConfig : public ConfigBase
{
public:
	ushort	RenderPeriod;	// 生成历史数据周期。默认30s
	ushort	ReportPeriod;	// 上报数据周期。默认300s
	ushort	StorePeriod;	// 写入Store周期。默认600s

	ushort	MaxCache;		// 缓存最大长度。默认16 * 1024
	ushort	MaxReport;		// 每次最大上报长度。默认1024

	byte	TagEnd;		// 数据区结束标识符

	GatherConfig();

	virtual void Init();

	// 注册给 TokenClient 名称 Gather/Set
	bool Set(const Pair& args, Stream& result);
	// 注册给 TokenClient 名称 Gather/Get
	bool Get(const Pair& args, Stream& result);

	static GatherConfig* Current;
	static GatherConfig* Create();
};

#endif
