#ifndef __Pair_H__
#define __Pair_H__

#include "Kernel\Sys.h"
#include "Message.h"

#include "Net\IPEndPoint.h"

// 名值对
class Pair
{
public:
	//Pair(const Pair& pair) = delete;

	virtual Buffer Get(cstring name) const = 0;
	//bool Set(cstring name, const Buffer& bs);
	//bool Set(const String& name, const Buffer& bs);

	virtual bool Get(cstring name, byte& value)		const = 0;
	virtual bool Get(cstring name, ushort& value)	const = 0;
	virtual bool Get(cstring name, uint& value)		const = 0;
	virtual bool Get(cstring name, UInt64& value)	const = 0;
	virtual bool Get(cstring name, Buffer& value)	const = 0;
	virtual bool Get(cstring name, IPEndPoint& value)	const = 0;

	virtual bool Set(cstring name, byte value)		= 0;
	virtual bool Set(cstring name, ushort value)	= 0;
	virtual bool Set(cstring name, uint value)		= 0;
	virtual bool Set(cstring name, UInt64 value)	= 0;
	virtual bool Set(cstring name, const String& str)	= 0;
	virtual bool Set(cstring name, const IPEndPoint& value)	= 0;

	// 字典名值对操作
	//IDictionary GetAll() const;
	//bool Set(const IDictionary& dic);
};

#endif
