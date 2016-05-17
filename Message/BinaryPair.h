#ifndef __BinaryPair_H__
#define __BinaryPair_H__

#include "Sys.h"
#include "Message.h"

#include "Net\Net.h"

// 二进制名值对
class BinaryPair : public Object
{
public:

	//BinaryPair(Buffer& bs);
	BinaryPair(Stream& ms);
	BinaryPair(const BinaryPair& pair) = delete;

	Buffer Get(const char* name) const;
	bool Set(const char* name, const Buffer& bs);
	bool Set(const String& name, const Buffer& bs);

	bool Get(const char* name, byte& value) const;
	bool Get(const char* name, ushort& value) const;
	bool Get(const char* name, uint& value) const;
	bool Get(const char* name, UInt64& value) const;
	bool Get(const char* name, Buffer& value) const;
	bool Get(const char* name, IPEndPoint& value) const;

	bool Set(const char* name, byte value);
	bool Set(const char* name, ushort value);
	bool Set(const char* name, uint value);
	bool Set(const char* name, UInt64 value);
	bool Set(const char* name, const IPEndPoint& value);

private:
	//byte*	Data;		// 数据指针
	//uint	Length;		// 数据长度
	uint	_p;	// 写入时的位置
	Stream*	_s;
};

#endif
