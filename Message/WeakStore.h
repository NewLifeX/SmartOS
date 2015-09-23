#ifndef __WeakStore_H__
#define __WeakStore_H__

#include "Sys.h"
#include "Stream.h"

// 弱存储
class WeakStore
{
public:
	int			Magic;	// 幻数。用于唯一标识
	ByteArray	Data;	// 数据

	// 初始化
	WeakStore(int magic = 0, byte* ptr = NULL, uint len = 0);

	// 检查并确保初始化，返回原来是否已初始化
	bool Check();
	void Init();

    // 重载索引运算符[]，让它可以像数组一样使用下标索引。
    byte& operator[](int i) const;
};

#endif
