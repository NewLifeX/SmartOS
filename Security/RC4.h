#ifndef __RC4_H__
#define __RC4_H__

#include "Sys.h"

// RC4 加密算法
class RC4
{
public:
	static int KeyLength = 256;

	// 加解密
	static void Encrypt(byte* data, uint len, byte* pass, uint plen);
};

#endif
