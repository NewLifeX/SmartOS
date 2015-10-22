#ifndef __Config_H__
#define __Config_H__

#include "Sys.h"
#include "Stream.h"
#include "Storage\Storage.h"

class ConfigBlock
{
private:
    static const uint c_Version = 0x534F5453; // STOS

public:
	static Storage*	Device;
	static uint		BaseAddress;

public:
	uint	Signature;
	ushort	HeaderCRC;
	ushort	DataCRC;
	uint	Size;
	char	Name[4];

    bool IsGoodBlock() const;
    bool IsGoodData () const;

    const ConfigBlock*	Next() const;
    const void*			Data() const;

    bool Init(const char* name, const ByteArray& bs);
    bool Write(Storage* storage, uint addr, const ByteArray& bs);

	// 查找
    static const ConfigBlock* Find(const char* name, uint addr = NULL, bool fAppend = false);
    // 废弃
	static bool Invalid(const char* name, uint addr = NULL, Storage* storage = NULL);
    // 设置配置数据
    static const void* Set(const char* name, const ByteArray& bs, uint addr = NULL, Storage* storage = NULL);
	// 获取配置数据
    static bool Get(const char* name, ByteArray& bs, uint addr = NULL);
	// 获取配置数据
    static const void* Get(const char* name, uint addr = NULL);
};

// 必须设定为1字节对齐，否则offsetof会得到错误的位置
#pragma pack(push)	// 保存对齐状态
// 强制结构体紧凑分配空间
#pragma pack(1)

// 配置信息
class TConfig
{
public:
	ushort		Length;		// 数据长度

	byte		HardVer;	// 硬件版本
	byte		SoftVer;	// 软件版本
	ushort		Kind;		// 类型
	byte		Address;	// 分配得到的设备地址
	byte		Password[16];	// 通信密码
	byte		Server;		// 网关ID
	byte		Channel;	// 通道
	ushort		Speed;		// 传输速度
	byte		ServerKey[16];	// 服务端组网密码，退网时使用。一般6字节

	ushort		PingTime;	// 心跳时间。秒
	ushort		OfflineTime;	// 离线阀值时间。秒
	ushort		SleepTime;	// 睡眠时间。秒

	// 初始化，各字段为0
	TConfig();
	void LoadDefault();
};

#pragma pack(pop)	// 恢复对齐状态

//extern TConfig Config;

/*
配置子系统，链式保存管理多配置段。
1，每个配置段都有固定长度的头部，包括签名、校验、名称等，数据紧跟其后
2，借助签名和双校验确保是有效配置段
3，根据名称查找更新配置段
*/

#endif
