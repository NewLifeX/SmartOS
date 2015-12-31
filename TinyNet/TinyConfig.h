#ifndef __TinyConfig_H__
#define __TinyConfig_H__

#include "Sys.h"
#include "Stream.h"
#include "Config.h"

// 必须设定为1字节对齐，否则offsetof会得到错误的位置
#pragma pack(push)	// 保存对齐状态
// 强制结构体紧凑分配空间
#pragma pack(1)

// 配置信息
class TinyConfig
{
public:
	byte	Length;		// 数据长度

	byte	OfflineTime;// 离线阀值时间。秒
	byte	SleepTime;	// 睡眠时间。秒
	byte	PingTime;	// 心跳时间。秒

	ushort	Kind;		// 类型
	byte	Address;	// 分配得到的设备地址
	byte	Server;		// 网关ID
	byte	Channel;	// 通道
	ushort	Speed;		// 传输速度
	byte	HardVer;	// 硬件版本
	byte	SoftVer;	// 软件版本

	byte	Password[16]; // 通信密码
	byte	Mac[6];		// 无线物理地址

	TinyConfig();
	void LoadDefault();

	void Load();
	void Save();
	void Clear();

	// 序列化到消息数据流
	void Write(Stream& ms) const;
	void Read(Stream& ms);

	static TinyConfig* Current;
	static TinyConfig* Init();
	
private:
	Config*	Cfg;
};

#pragma pack(pop)	// 恢复对齐状态

#endif
