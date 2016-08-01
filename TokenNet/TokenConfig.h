#ifndef __TokenConfig_H__
#define __TokenConfig_H__

#include "Config.h"
#include "Net\NetUri.h"

// 必须设定为1字节对齐，否则offsetof会得到错误的位置
//#pragma pack(push)	// 保存对齐状态
// 强制结构体紧凑分配空间
//#pragma pack(1)

// 配置信息
class TokenConfig : public ConfigBase
{
public:
	byte	Length;			// 数据长度

	char	_User[16];		// 登录名
	char	_Pass[32];		// 登录密码
	ushort	HardVer;		// 硬件版本
	ushort	SoftVer;		// 软件版本

	byte	PingTime;		// 心跳时间。秒
	NetType	Protocol;		// 协议，TCP=6/UDP=17
	ushort	Port;			// 本地端口
	uint	ServerIP;		// 服务器IP地址。服务器域名解析成功后覆盖
	ushort	ServerPort;		// 服务器端口
	char	_Token[32];		// 访问服务器令牌
	char	_Server[32];	// 服务器域名。出厂为空，从厂商服务器覆盖，恢复出厂设置时清空
	char	_Vendor[32];	// 厂商服务器域名。原始厂商服务器地址

	byte	TagEnd;		// 数据区结束标识符

	TokenConfig();
	virtual void Init();
	virtual void Show() const;

	String	User()		{ return String(_User, sizeof(_User), false); }
	String	Pass()		{ return String(_Pass, sizeof(_Pass), false); }
	String	Token()		{ return String(_Token, sizeof(_Token), false); }
	String	Server()	{ return String(_Server, sizeof(_Server), false); }
	String	Vendor()	{ return String(_Vendor, sizeof(_Vendor), false); }
	NetUri	Uri() const;

	static TokenConfig* Current;
	static TokenConfig*	Create(cstring vendor, NetType protocol, ushort sport, ushort port);

private:
};

//#pragma pack(pop)	// 恢复对齐状态

#endif
