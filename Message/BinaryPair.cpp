#include "BinaryPair.h"

// 初始化消息，各字段为0
/*BinaryPair::BinaryPair(Buffer& bs)
{
	Data	= bs.GetBuffer();
	Length	= bs.Length();
	Position	= 0;
}*/

BinaryPair::BinaryPair(Stream& ms)
{
	_s	= &ms;
	_p	= ms.Position();
	_canWrite	= true;
}

BinaryPair::BinaryPair(const Stream& ms)
{
	_s	= (Stream*)&ms;
	_p	= ms.Position();
	_canWrite	= false;
}

Buffer BinaryPair::Get(cstring name) const
{
	Buffer err(nullptr, 0);

	// 暂时不方便支持空名称的名值对，而服务端是支持的
	if(!name) return err;

	// 从当前位置开始向后找，如果找不到，再从头开始找到当前位置。
	// 这样子安排，如果是顺序读取，将会提升性能

	//int slen	= strlen(name);
	String sn	= name;

	auto& ms	= *_s;
	uint p	= ms.Position();
	for(int i=0; i<2; i++)
	{
		while(ms.Remain() && (i ==0 || ms.Position() < p))
		{
			// 每个名值对最小长度是2，名称和值的长度都为0
			if(ms.Remain() < 2) return err;
			int len	= ms.ReadEncodeInt();
			if(len <0 || len > ms.Remain()) return err;
			auto nm	= ms.ReadBytes(len);

			if(ms.Remain() < 1) return err;
			int ln2	= ms.ReadEncodeInt();
			if(ln2 <0 || ln2 > ms.Remain()) return err;
			auto dt	= ms.ReadBytes(ln2);

			if(sn == String((cstring)nm, len)) return Buffer(dt, ln2);
		}

		// 从头开始再来一次
		if(p == _p) break;

		ms.SetPosition(_p);
	}

	return err;
}

bool BinaryPair::Set(cstring name, const Buffer& bs)
{
	return Set(String(name), bs);
}

bool BinaryPair::Set(const String& name, const Buffer& bs)
{
	if(!_canWrite) return false;

	auto& ms	= *_s;
	ms.WriteArray(name);
	ms.WriteArray(bs);

	return true;
}

bool BinaryPair::Get(cstring name, byte& value) const
{
	auto bs	= Get(name);
	if(!bs.Length()) return false;

	value	= bs[0];

	return true;
}

bool BinaryPair::Get(cstring name, ushort& value) const
{
	auto bs	= Get(name);
	if(!bs.Length()) return false;

	value	= bs.ToUInt16();

	return true;
}

bool BinaryPair::Get(cstring name, uint& value) const
{
	auto bs	= Get(name);
	if(!bs.Length()) return false;

	value	= bs.ToUInt32();

	return true;
}

bool BinaryPair::Get(cstring name, UInt64& value) const
{
	auto bs	= Get(name);
	if(!bs.Length()) return false;

	value	= bs.ToUInt64();

	return true;
}

bool BinaryPair::Get(cstring name, Buffer& value) const
{
	auto bs	= Get(name);
	if(!bs.Length()) return false;

	value	= bs;

	return true;
}

bool BinaryPair::Get(cstring name, IPEndPoint& value) const
{
	auto bs	= Get(name);
	if(bs.Length() < 6) return false;

	if (bs[0] != 4) return false;	// 单片机这边不支持ipv6

	Buffer bs2(bs.GetBuffer() + 1, 6);
	value	= bs2;

	return true;
}


bool BinaryPair::Set(cstring name, byte value)
{
	return Set(name, Buffer(&value, 1));
}

bool BinaryPair::Set(cstring name, ushort value)
{
	return Set(name, Buffer(&value, 2));
}

bool BinaryPair::Set(cstring name, uint value)
{
	return Set(name, Buffer(&value, 4));
}

bool BinaryPair::Set(cstring name, UInt64 value)
{
	return Set(name, Buffer(&value, 8));
}

bool BinaryPair::Set(cstring name, const IPEndPoint& value)
{
	MemoryStream ms(7);

	ms.Write((byte)4);
	ms.Write(value.ToArray());

	return Set(name, Buffer(ms.GetBuffer(), ms.Position()));

	//return Set(name, value.ToArray());
}
