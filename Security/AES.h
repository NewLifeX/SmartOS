#ifndef __AES_H__
#define __AES_H__

#include "Sys.h"

// AES 加密算法
class AES
{
public:
	// 加解密
	static ByteArray Encrypt(const Array& data, const Array& pass);
	static ByteArray Decrypt(const Array& data, const Array& pass);
};

#endif
