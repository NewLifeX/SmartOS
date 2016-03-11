#ifndef __RSA_H__
#define __RSA_H__

#include "Sys.h"

// RSA 加密算法
class RSA
{
public:
	// 加解密
	static ByteArray Encrypt(const Buffer& data, const Buffer& pass);
	static ByteArray Decrypt(const Buffer& data, const Buffer& pass);
};

#endif
