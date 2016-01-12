#ifndef __TokenConfig_H__
#define __TokenConfig_H__

#include "Sys.h"
#include "Stream.h"
#include "Config.h"

// 必须设定为1字节对齐，否则offsetof会得到错误的位置
#pragma pack(push)	// 保存对齐状态
// 强制结构体紧凑分配空间
#pragma pack(1)

// 配置信息
class TokenConfig : public ConfigBase
{
public:
	byte	Length;			// 数据长度
	
	char	Name[16];		// 登录名
	char	Key[16];		// 登录密码
	ushort	HardVer;		// 硬件版本
	ushort	SoftVer;		// 软件版本

	byte	PingTime;		// 心跳时间。秒
	byte	Protocol;		// 协议，TCP=1/UDP=2
	ushort	Port;			// 本地端口
	uint	ServerIP;		// 服务器IP地址。服务器域名解析成功后覆盖
	ushort	ServerPort;		// 服务器端口
	char	Server[32];		// 服务器域名。出厂为空，从厂商服务器覆盖，恢复出厂设置时清空
	char	Vendor[32];		// 厂商服务器域名。原始厂商服务器地址
	
	TokenConfig();
	virtual void Init();
	virtual void Show() const;

	static TokenConfig* Current;
	static TokenConfig*	Create(const char* vendor, byte protocol, ushort sport, ushort port);
	
private:
	byte	TagEnd;		// 数据区结束标识符
};

#pragma pack(pop)	// 恢复对齐状态

#endif
