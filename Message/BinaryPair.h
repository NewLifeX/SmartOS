#ifndef __BinaryPair_H__
#define __BinaryPair_H__

#include "Sys.h"
#include "Stream.h"
#include "Message.h"

#include "Net\Net.h"

// 二进制名值对
class BinaryPair : public Object
{
public:

	BinaryPair(Buffer& bs);
	BinaryPair(Stream& ms);
	BinaryPair(const BinaryPair& pair) = delete;

	Buffer Get(const String& name) const;
	bool Set(const String& name, const Buffer& bs);

	bool Get(const String& name, byte& value) const;
	bool Get(const String& name, ushort& value) const;
	bool Get(const String& name, uint& value) const;
	bool Get(const String& name, UInt64& value) const;
	bool Get(const String& name, Buffer& value) const;
	bool Get(const String& name, IPEndPoint& value) const;

	bool Set(const String& name, byte value);
	bool Set(const String& name, ushort value);
	bool Set(const String& name, uint value);
	bool Set(const String& name, UInt64 value);
	//bool Set(const String& name, const Buffer& value);
	bool Set(const String& name, const IPEndPoint& value);

private:
	byte*	Data;		// 数据指针
	uint	Length;		// 数据长度
	uint	Position;	// 写入时的位置
};

#endif
