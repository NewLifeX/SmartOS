#ifndef __Config_H__
#define __Config_H__

#include "Sys.h"
#include "Stream.h"
#include "Storage\Storage.h"

// 配置管理
// 配置区以指定签名开头，后续链式跟随各配置块
class Config
{
private:

public:
	Storage*	Device;
	uint		Address;

	Config(Storage* st, uint addr);

	// 查找。size不为0时表示要查找该大小的合适配置块
    const void* Find(const char* name, int size = 0);
    // 废弃。仅清空名称，并不删除数据区
	bool Invalid(const char* name);
    // 设置配置数据
    const void* Set(const char* name, const ByteArray& bs);
	// 获取配置数据
    bool Get(const char* name, ByteArray& bs);
	// 获取配置数据，如果不存在则覆盖
    bool GetOrSet(const char* name, ByteArray& bs);
	// 获取配置数据
    const void* Get(const char* name);

	// 当前
	static Config* Current;
	// Flash最后一块作为配置区
	static Config& CreateFlash();
	// RAM最后一小段作为热启动配置区
	static Config& CreateRAM();
};

// 必须设定为1字节对齐，否则offsetof会得到错误的位置
#pragma pack(push)	// 保存对齐状态
// 强制结构体紧凑分配空间
#pragma pack(1)

// 系统配置信息
class HotConfig
{
public:
	ushort	Times;		// 启动次数

	void* Next() const;

	static HotConfig& Current();
};

// 系统配置信息
class SysConfig
{
public:

};

#pragma pack(pop)	// 恢复对齐状态

/*
配置子系统，链式保存管理多配置段。
1，每个配置段都有固定长度的头部，包括签名、校验、名称等，数据紧跟其后
2，借助签名和双校验确保是有效配置段
3，根据名称查找更新配置段
*/

#endif
