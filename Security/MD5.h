#ifndef __MD5_H__
#define __MD5_H__

#include "Sys.h"

// MD5 散列算法
class MD5
{
public:
	// 散列
	static ByteArray Hash(const ByteArray& data);
};

#endif
