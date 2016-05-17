#ifndef __WeakStore_H__
#define __WeakStore_H__

#include "Sys.h"

// 弱存储。
// 采用全局变量或者堆分配的变量在系统启动时会被清空。
// 弱存储在main函数内栈分配，位于栈的最后部分，不会因为重启而被清空
class WeakStore
{
public:
	const char*	Magic;	// 幻数。用于唯一标识
	uint		MagicLength;
	ByteArray	Data;	// 数据

	// 初始化
	WeakStore(const char* magic = nullptr, byte* ptr = nullptr, uint len = 0);

	// 检查并确保初始化，返回原来是否已初始化
	bool Check();
	void Init();

    // 重载索引运算符[]，定位到数据部分的索引。
    byte operator[](int i) const;
    byte& operator[](int i);
};

#endif
