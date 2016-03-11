#ifndef __RC6_H__
#define __RC6_H__

#include "Sys.h"

// RC6 加密算法
class RC6
{
public:
	// 加解密
	static ByteArray Encrypt(const Buffer& data, const Buffer& pass);
	static ByteArray Decrypt(const Buffer& data, const Buffer& pass);
};

#endif
