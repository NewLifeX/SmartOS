#ifndef __HistoryStore_H__
#define __HistoryStore_H__

#include "Core\\Delegate.h"

// 历史数据存储
class HistoryStore
{
public:
	MemoryStream	Cache;	// 数据
	short	RenderPeriod;	// 生成历史数据周期。默认30s
	short	ReportPeriod;	// 上报数据周期。默认300s
	short	StorePeriod;	// 写入Store周期。默认600s

	short	MaxCache;		// 缓存最大长度。默认16 * 1024
	short	MaxReport;		// 每次最大上报长度。默认1024

	bool Opened;

	// 数据上报句柄
	DataHandler OnReport;
	// 数据存储句柄
	DataHandler OnStore;

	// 初始化
	HistoryStore();
	~HistoryStore();

	void Set(void* data, int size);

	bool Open();
	void Close();

	// 写入一条历史数据
	int Write(const Buffer& bs);

	// 存储数据到Flash上指定地址
	static uint ReadFlash(uint address, Buffer& bs);
	static uint WriteFlash(uint address, const Buffer& bs);

private:
	void*	Data;
	int		Size;

	short	_Report;
	short	_Store;

	uint	_task;

	static void RenderTask(void* param);
	void Reader();
	void Report();
	void Store();

	void Process(int len, DataHandler handler);
};

/*
历史数据格式：4时间 + N数据
*/

#endif
