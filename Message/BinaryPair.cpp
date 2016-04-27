#include "BinaryPair.h"

// 初始化消息，各字段为0
BinaryPair::BinaryPair(Buffer& bs)
{
	Data	= bs.GetBuffer();
	Length	= bs.Length();
	Position	= 0;
}

BinaryPair::BinaryPair(Stream& ms)
{
	Data	= ms.GetBuffer();
	Length	= ms.Length;
	Position	= 0;
}

Buffer BinaryPair::Get(const String& name) const
{
	if(!Data || !Length) return Buffer(nullptr, 0);
	// 暂时不方便支持空名称的名值对，而服务端是支持的
	//if(!name) return Buffer(nullptr, 0);

	Stream ms(Data, Length);
	while(ms.Remain())
	{
		int len	= ms.ReadEncodeInt();
		auto nm	= ms.ReadBytes(len);
		int ln2	= ms.ReadEncodeInt();
		auto dt	= ms.ReadBytes(ln2);

		if(name == nm) return Buffer(dt, ln2);
	}

	return Buffer(nullptr, 0);
}

bool BinaryPair::Set(const String& name, const Buffer& bs)
{
	if(Position >= Length) return false;

	Stream ms(Data + Position, Length - Position);
	ms.WriteArray(name);
	ms.WriteArray(bs);

	return true;
}

bool BinaryPair::Get(const String& name, byte& value) const
{
	auto bs	= Get(name);
	if(!bs.Length()) return false;

	value	= bs[0];

	return true;
}

bool BinaryPair::Get(const String& name, ushort& value) const
{
	auto bs	= Get(name);
	if(!bs.Length()) return false;

	value	= bs.ToUInt16();

	return true;
}

bool BinaryPair::Get(const String& name, uint& value) const
{
	auto bs	= Get(name);
	if(!bs.Length()) return false;

	value	= bs.ToUInt32();

	return true;
}

bool BinaryPair::Get(const String& name, UInt64& value) const
{
	auto bs	= Get(name);
	if(!bs.Length()) return false;

	value	= bs.ToUInt64();

	return true;
}

bool BinaryPair::Get(const String& name, Buffer& value) const
{
	auto bs	= Get(name);
	if(!bs.Length()) return false;

	value	= bs;

	return true;
}

bool BinaryPair::Get(const String& name, IPEndPoint& value) const
{
	auto bs	= Get(name);
	if(bs.Length() < 6) return false;

	//value.Address	= bs;
	//value.Port		= bs.Sub(4, 2).ToUInt16();
	value	= bs;

	return true;
}


bool BinaryPair::Set(const String& name, byte value)
{
	return Set(name, Buffer(&value, 1));
}

bool BinaryPair::Set(const String& name, ushort value)
{
	return Set(name, Buffer(&value, 2));
}

bool BinaryPair::Set(const String& name, uint value)
{
	return Set(name, Buffer(&value, 4));
}

bool BinaryPair::Set(const String& name, UInt64 value)
{
	return Set(name, Buffer(&value, 8));
}

bool BinaryPair::Set(const String& name, const IPEndPoint& value)
{
	return Set(name, value.ToArray());
}

