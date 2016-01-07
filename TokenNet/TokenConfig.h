#ifndef __TokenConfig_H__
#define __TokenConfig_H__

#include "Sys.h"
#include "Stream.h"
#include "Storage\Storage.h"

// 必须设定为1字节对齐，否则offsetof会得到错误的位置
#pragma pack(push)	// 保存对齐状态
// 强制结构体紧凑分配空间
#pragma pack(1)

// 配置信息
struct TokenConfig
{
	byte	Length;			// 数据长度

	ushort	HardVer;		// 硬件版本
	ushort	SoftVer;		// 软件版本

	byte	PingTime;		// 心跳时间。秒

	byte	Protocol;		// 协议，TCP=1/UDP=2
	ushort	Port;			// 本地端口
	uint	ServerIP;		// 服务器IP地址。服务器域名解析成功后覆盖
	ushort	ServerPort;		// 服务器端口
	char	Server[32];		// 服务器域名。出厂为空，从厂商服务器覆盖，恢复出厂设置时清空
	char	Vendor[32];		// 厂商服务器域名。原始厂商服务器地址
	
	void LoadDefault();

	bool Load();
	void Save();
	void Show();

	// 序列化到消息数据流
	void Write(Stream& ms) const;
	void Read(Stream& ms);

	static TokenConfig* Current;
	static TokenConfig*	Init(const char* vendor, byte protocol, ushort sport, ushort port);
};

#pragma pack(pop)	// 恢复对齐状态

#endif
