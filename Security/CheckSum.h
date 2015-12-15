#ifndef __CheckSum_H__
#define __CheckSum_H__

#include "Sys.h"

//  相异和
class CheckSum
{
public:
	// 加解密
	static byte Sum(const Array& data,byte check);
};

#endif
