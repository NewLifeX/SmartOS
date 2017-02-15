#ifndef __MD5_H__
#define __MD5_H__

#include "Kernel\Sys.h"

// MD5 散列算法
class MD5
{
public:
	// 散列
	static ByteArray Hash(const Buffer& data);
	// 字符串散列。先取得字符串对应的字节码进行散列，然后转为HEX编码字符串
	static String Hash(const String& str);
};

#endif
