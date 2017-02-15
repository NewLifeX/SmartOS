#ifndef __BinaryPair_H__
#define __BinaryPair_H__

#include "Kernel\Sys.h"
#include "Pair.h"

#include "Net\IPEndPoint.h"

// 二进制名值对
class BinaryPair : public Object, public Pair
{
public:
	//BinaryPair(Buffer& bs);
	BinaryPair(Stream& ms);
	BinaryPair(const Stream& ms);
	BinaryPair(const BinaryPair& pair) = delete;

	virtual Buffer Get(cstring name) const;
	bool Set(cstring name, const Buffer& bs);
	bool Set(const String& name, const Buffer& bs);

	virtual bool Get(cstring name, byte& value) const;
	virtual bool Get(cstring name, ushort& value) const;
	virtual bool Get(cstring name, uint& value) const;
	virtual bool Get(cstring name, UInt64& value) const;
	virtual bool Get(cstring name, Buffer& value) const;
	virtual bool Get(cstring name, IPEndPoint& value) const;

	virtual bool Set(cstring name, byte value);
	virtual bool Set(cstring name, ushort value);
	virtual bool Set(cstring name, uint value);
	virtual bool Set(cstring name, UInt64 value);
	virtual bool Set(cstring name, const String& str);
	virtual bool Set(cstring name, const IPEndPoint& value);

	// 字典名值对操作
	IDictionary GetAll() const;
	bool Set(const IDictionary& dic);
	
private:
	uint	_p;	// 写入时的位置
	Stream*	_s;
	bool	_canWrite;
};

#endif
