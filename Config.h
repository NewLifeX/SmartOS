#ifndef __Config_H__
#define __Config_H__

#include "Sys.h"
#include "Stream.h"

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
	ushort		Type;		// 类型
	byte		Address;	// 分配得到的设备地址
	byte		Server;		// 网关ID
	byte		Channel;	// 通道
	byte		Speed;		// 传输速度
	byte		ServerKey[16];	// 服务端组网密码，退网时使用。一般6字节

	ushort		PingTime;	// 心跳时间。秒
	ushort		OfflineTime;	// 离线阀值时间。秒
	ushort		SleepTime;	// 睡眠时间。秒

	// 初始化，各字段为0
	void Init();
	void LoadDefault();
};

#pragma pack(pop)	// 恢复对齐状态

extern TConfig Config;

#endif
