#ifndef __Config_H__
#define __Config_H__

#include "Sys.h"
#include "Stream.h"

// 必须设定为1字节对齐，否则offsetof会得到错误的位置
#pragma pack(push)	// 保存对齐状态
// 强制结构体紧凑分配空间
#pragma pack(1)

// 配置信息
class Config
{
public:
	int		Length;	// 数据长度

	// 初始化，各字段为0
	Config();
};

#pragma pack(pop)	// 恢复对齐状态

#endif
