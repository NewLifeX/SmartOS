#include <stddef.h>

#include "_Core.h"

#include "ByteArray.h"
#include "SString.h"

#include "Stream.h"

extern ushort _REV16(ushort);
extern uint _REV(uint);

// 使用缓冲区初始化数据流。注意，此时指针位于0，而内容长度为缓冲区长度
Stream::Stream(void* buf, uint len)
{
	Init(buf, len);
}

Stream::Stream(const void* buf, uint len)
{
	Init((void*)buf, len);

	CanWrite	= false;
}

// 使用字节数组初始化数据流。注意，此时指针位于0，而内容长度为缓冲区长度
Stream::Stream(Buffer& bs)
{
	Init(bs.GetBuffer(), bs.Length());
}

Stream::Stream(const Buffer& bs)
{
	Init((void*)bs.GetBuffer(), bs.Length());

	CanWrite	= false;
}

void Stream::Init(void* buf, uint len)
{
	assert_ptr(buf);

	_Buffer		= (byte*)buf;
	_Capacity	= len;
	_Position	= 0;
	CanWrite	= true;
	CanResize	= true;
	Length		= len;
	Little		= true;
}

bool Stream::CheckRemain(uint count)
{
	uint remain = _Capacity - _Position;
	// 容量不够，需要扩容
	if(count > remain)
	{
		if(CanResize)
			debug_printf("数据流 0x%p 剩余容量 (%d - %d) = %d 不足 %d ，无法扩容！\r\n", this, _Capacity, _Position, remain, count);
		else
			assert(false, "无法扩容");

		return false;
	}

	return true;
}

// 数据流容量
uint Stream::Capacity() const { return _Capacity; }
void Stream::SetCapacity(uint len) { CheckRemain(len - _Position); }

// 当前位置
uint Stream::Position() const { return _Position; }

// 设置位置
bool Stream::SetPosition(int p)
{
	// 允许移动到最后一个字节之后，也就是Length
	//assert(p <= Length, "设置的位置超出长度");
	if(p < 0 && p > Length)
	{
		debug_printf("设置的位置 %d 超出长度 %d\r\n", p, Length);
		return false;
	}

	_Position = p;
	return true;
}

// 余下的有效数据流长度。0表示已经到达终点
uint Stream::Remain() const { return Length - _Position; };

// 尝试前后移动一段距离，返回成功或者失败。如果失败，不移动游标
bool Stream::Seek(int offset)
{
	if(offset == 0) return true;

	return SetPosition(offset + _Position);
}

// 数据流指针。注意：扩容后指针会改变！
byte* Stream::GetBuffer() const { return _Buffer; }

// 数据流当前位置指针。注意：扩容后指针会改变！
byte* Stream::Current() const { return &_Buffer[_Position]; }

// 读取7位压缩编码整数
uint Stream::ReadEncodeInt()
{
	uint value = 0;
	// 同时计算最大4字节，避免无限读取错误数据
	for(int i = 0, k = 0; i < 4; i++, k += 7)
	{
		int temp = ReadByte();
		if(temp < 0) break;

		value |= (temp & 0x7F) << k;
		if((temp & 0x80) == 0) break;
	}
	return value;
}

// 读取数据到字节数组，由字节数组指定大小。不包含长度前缀
uint Stream::Read(Buffer& bs)
{
	if(bs.Length() == 0) return 0;

	Buffer ss(_Buffer, Length);
	int count	= bs.Copy(0, ss, _Position, bs.Length());

	// 游标移动
	_Position += count;

	bs.SetLength(count);

	return count;
}

// 写入7位压缩编码整数
uint Stream::WriteEncodeInt(uint value)
{
	if(!CanWrite) return 0;

	byte buf[8];
	int k	= 0;
	for(int i = 0; i < 4 && value >= 0x80; i++)
	{
		buf[k++]	= (byte)(value | 0x80);

		value	>>= 7;
	}
	{
		buf[k++] = (byte)value;
	}

	return Write(Buffer(buf, k));
}

// 把字节数组的数据写入到数据流。不包含长度前缀
bool Stream::Write(const Buffer& bs)
{
	int count	= bs.Length();
	if(count == 0) return true;
	if(!CanWrite) return false;
	if(!CheckRemain(count)) return false;

	Buffer ss(_Buffer, _Capacity);
	count	= ss.Copy((uint)_Position, bs, 0, count);

	_Position += count;
	// 内容长度不是累加，而是根据位置而扩大
	if(_Position > Length) Length = _Position;

	return true;
}

// 读取指定长度的数据并返回首字节指针，移动数据流位置
byte* Stream::ReadBytes(int count)
{
	// 默认小于0时，读取全部数据
	if(count < 0) count = Remain();

	byte* p = Current();
	if(!Seek(count)) return nullptr;

	return p;
}

// 读取一个字节，不移动游标。如果没有可用数据，则返回-1
int Stream::Peek() const
{
	if(!Remain()) return -1;
	return *Current();
}

// 从数据流读取变长数据到字节数组。以压缩整数开头表示长度
uint Stream::ReadArray(Buffer& bs)
{
	uint len = ReadEncodeInt();
	if(!len)
	{
		bs.SetLength(0);

		return 0;
	}

	if(len > bs.Length() && !bs.SetLength(len))
	{
		// 在设计时，如果取得的长度超级大，可能是设计错误
		//if(len > 0x40)
		{
			debug_printf("Stream::ReadArray 缓冲区大小不足 读 %d > %d ", len, bs.Length());
			//assert(len <= bs.Capacity(), "缓冲区大小不足");
		}
		return 0;
	}

	return Read(bs);
}

ByteArray Stream::ReadArray(int count)
{
	ByteArray bs;
	bs.SetLength(count);
	Read(bs);

	return bs;
}

// 读取字节数组返回。不用担心有临时变量的构造和析构，RVO会为我们排忧解难
ByteArray Stream::ReadArray()
{
	ByteArray bs;
	ReadArray(bs);

	return bs;
}

// 把字节数组作为变长数据写入到数据流。以压缩整数开头表示长度
bool Stream::WriteArray(const Buffer& bs)
{
	WriteEncodeInt(bs.Length());
	return Write(bs);
}

String Stream::ReadString()
{
	String str;

	ReadArray(str);

	return str;
}

int	Stream::ReadByte()
{
	if(Length == _Position) return -1;

	byte* p = Current();
	if(!Seek(1)) return 0;

	return *p;
}

ushort	Stream::ReadUInt16()
{
	ushort v;
	Buffer bs(&v, sizeof(v));
	if(!Read(bs)) return 0;
	if(!Little) v = _REV16(v);
	return v;
}

uint	Stream::ReadUInt32()
{
	uint v;
	Buffer bs(&v, sizeof(v));
	if(!Read(bs)) return 0;
	if(!Little) v = _REV(v);
	return v;
}

UInt64	Stream::ReadUInt64()
{
	UInt64 v;
	Buffer bs(&v, sizeof(v));
	if(!Read(bs)) return 0;
	if(!Little) v = _REV(v >> 32) | ((UInt64)_REV(v & 0xFFFFFFFF) << 32);
	return v;
}

bool Stream::Write(byte value)
{
	return Write(Buffer(&value, sizeof(value)));
}

bool Stream::Write(ushort value)
{
	if(!Little) value = _REV16(value);

	return Write(Buffer(&value, sizeof(value)));
}

bool Stream::Write(uint value)
{
	if(!Little) value = _REV(value);

	return Write(Buffer(&value, sizeof(value)));
}

bool Stream::Write(UInt64 value)
{
	if(!Little) value = _REV(value >> 32) | ((UInt64)_REV(value & 0xFFFFFFFF) << 32);

	return Write(Buffer(&value, sizeof(value)));
}

/******************************** MemoryStream ********************************/

MemoryStream::MemoryStream(uint len) : Stream(_Arr, ArrayLength(_Arr))
{
	Length	= 0;
	_needFree	= false;
	if(len > ArrayLength(_Arr))
	{
		byte* buf	= new byte[len];
		Init(buf, len);
		_needFree	= true;
	}
}

MemoryStream::MemoryStream(void* buf, uint len) : Stream(buf, len)
{
	_needFree	= false;
}

// 销毁数据流
MemoryStream::~MemoryStream()
{
	assert_ptr(this);
	if(_needFree)
	{
		if(_Buffer != _Arr) delete[] _Buffer;
		_Buffer = nullptr;
	}
}

bool MemoryStream::CheckRemain(uint count)
{
	uint remain = _Capacity - _Position;
	// 容量不够，需要扩容
	if(count > remain)
	{
		if(!CanResize) return Stream::CheckRemain(count);

		// 原始容量成倍扩容
		uint total = _Position + count;
		uint size = _Capacity;
		while(size < total) size <<= 1;

		// 申请新的空间，并复制数据
		byte* bufNew = new byte[size];
		if(Length > 0) Buffer(_Buffer, Length).CopyTo(0, bufNew, -1);

		if(_Buffer != _Arr) delete[] _Buffer;

		_Buffer		= bufNew;
		_Capacity	= size;
		_needFree	= true;
	}

	return true;
}

#if defined(__CC_ARM)
__weak ushort _REV16(ushort value)
{
	return (ushort)((value << 8) | (value >> 8));
}

__weak uint _REV(uint value)
{
	return (_REV16(value & 0xFFFF) << 16) | (_REV16(value >> 16) >> 16);
}
#elif defined(__GNUC__)
ushort _REV16(ushort value) __attribute__((weak));
ushort _REV16(ushort value)
{
	return (ushort)((value << 8) | (value >> 8));
}

uint _REV(uint value) __attribute__((weak));
uint _REV(uint value)
{
	return (_REV16(value & 0xFFFF) << 16) | (_REV16(value >> 16) >> 16);
}
#endif
