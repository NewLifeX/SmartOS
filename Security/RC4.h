#ifndef __RC4_H__
#define __RC4_H__

#include "Sys.h"

// RC4 加密算法
class RC4
{
public:
	// 加解密
	static void Encrypt(ByteArray& data, const ByteArray& pass);
};

#endif
