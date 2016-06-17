#ifndef __BinaryPair_H__
#define __BinaryPair_H__

#include "Sys.h"
#include "Message.h"

#include "Net\IPEndPoint.h"

// 二进制名值对
class BinaryPair : public Object
{
public:
	//BinaryPair(Buffer& bs);
	BinaryPair(Stream& ms);
	BinaryPair(const Stream& ms);
	BinaryPair(const BinaryPair& pair) = delete;

	Buffer Get(cstring name) const;
	bool Set(cstring name, const Buffer& bs);
	bool Set(const String& name, const Buffer& bs);

	bool Get(cstring name, byte& value) const;
	bool Get(cstring name, ushort& value) const;
	bool Get(cstring name, uint& value) const;
	bool Get(cstring name, UInt64& value) const;
	bool Get(cstring name, Buffer& value) const;
	bool Get(cstring name, IPEndPoint& value) const;

	bool Set(cstring name, byte value);
	bool Set(cstring name, ushort value);
	bool Set(cstring name, uint value);
	bool Set(cstring name, UInt64 value);
	bool Set(cstring name, const String& str);
	bool Set(cstring name, const IPEndPoint& value);

	// 字典名值对操作
	Dictionary GetAll() const;
	bool Set(const Dictionary& dic);
	
private:
	uint	_p;	// 写入时的位置
	Stream*	_s;
	bool	_canWrite;
};

#endif
