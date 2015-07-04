#ifndef __MD5_H__
#define __MD5_H__

#include "Sys.h"

// MD5 散列算法
class MD5
{
public:
	// 散列
	static void Hash(const ByteArray& data, ByteArray& hash);
};

#endif
