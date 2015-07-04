#include "Stream.h"

Stream::Stream(uint len)
{
	if(len <= ArrayLength(_Arr))
	{
		len = ArrayLength(_Arr);
		_Buffer = _Arr;
		_needFree = false;
	}
	else
	{
		_Buffer = new byte[len];
		assert_ptr(_Buffer);
		_needFree = true;
	}

	_Capacity	= len;
	_Position	= 0;
	Length		= len;
}

// 使用缓冲区初始化数据流。注意，此时指针位于0，而内容长度为缓冲区长度
Stream::Stream(byte* buf, uint len)
{
	assert_ptr(buf);
	//assert_param2(len > 0, "不能用0长度缓冲区来初始化数据流");

	_Buffer = buf;
	_Capacity = len;
	_Position = 0;
	_needFree = false;
	Length = len;
}

// 使用字节数组初始化数据流。注意，此时指针位于0，而内容长度为缓冲区长度
Stream::Stream(ByteArray& bs)
{
	_Buffer = bs.GetBuffer();
	_Capacity = bs.Length();
	_Position = 0;
	_needFree = false;
	Length = bs.Length();
}

// 销毁数据流
Stream::~Stream()
{
	assert_ptr(this);
	if(_needFree)
	{
		if(_Buffer != _Arr) delete[] _Buffer;
		_Buffer = NULL;
	}
}

// 数据流容量
uint Stream::Capacity() const { return _Capacity; }

// 当前位置
uint Stream::Position() const { return _Position; }

// 设置位置
bool Stream::SetPosition(int p)
{
	// 允许移动到最后一个字节之后，也就是Length
	//assert_param2(p <= Length, "设置的位置超出长度");
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

// 从当前位置读取数据
uint Stream::Read(byte* buf, uint offset, int count)
{
	assert_param2(buf, "从数据流读取数据需要有效的缓冲区");

	if(count == 0) return 0;

	uint remain = Remain();
	if(count < 0)
		count = remain;
	else if(count > remain)
		count = remain;

	// 复制需要的数据
	memcpy(buf + offset, Current(), count);

	// 游标移动
	_Position += count;

	return count;
}

uint Stream::ReadEncodeInt()
{
	uint value = 0;
	byte temp = 0;
	for(int i = 0, k = 0; i < 4; i++, k += 7)
	{
		temp = Read<byte>();
		if(temp < 0x7F)
		{
			value |= (temp << k);
			return value;
		}
		temp &= 0x7F;
		value |= (temp << k);
	}
	return 0xFFFFFFFF;
}

// 把数据写入当前位置
bool Stream::Write(byte* buf, uint offset, uint count)
{
	assert_param2(buf, "向数据流写入数据需要有效的缓冲区");

	if(!CheckRemain(count)) return false;

	memcpy(Current(), buf + offset, count);

	_Position += count;
	//Length += count;
	// 内容长度不是累加，而是根据位置而扩大
	if(_Position > Length) Length = _Position;
	
	return true;
}

uint Stream::WriteEncodeInt(uint value)
{
	byte temp;
	for( int i = 0 ; i < 4 ; i++ )
	{
		temp = (byte)value;
		if(temp < 0x7F)
		{
			Write(&temp, 0, 1);
			return i + 1;
		}
		temp |= 0x80;
		Write(&temp, 0, 1);
		value >>= 7;
	}
	return 0;
}

// 写入字符串，先写入压缩编码整数表示的长度
uint Stream::Write(string str)
{
	int len = 0;
	string p = str;
	while(*p++) len++;

	WriteEncodeInt(len);
	if(len) Write((byte*)str, 0, len);

	return len;
}

bool Stream::Write(const ByteArray& bs)
{
	return Write(bs.GetBuffer(), 0, bs.Length());
}

byte* Stream::ReadBytes(int count)
{
	// 默认小于0时，读取全部数据
	if(count < 0) count = Remain();

	byte* p = Current();
	if(!Seek(count)) return NULL;

	return p;
}

// 读取一个字节，不移动游标。如果没有可用数据，则返回-1
int Stream::Peek() const
{
	if(!Remain()) return -1;
	return *Current();
}

bool Stream::CheckRemain(uint count)
{
	uint remain = _Capacity - _Position;
	// 容量不够，需要扩容
	if(count > remain)
	{
		if(!_needFree && _Buffer != _Arr)
		{
			debug_printf("数据流 0x%08X 剩余容量 %d 不足 %d ，而外部缓冲区无法扩容！", this, remain, count);
			//assert_param(false);
			return false;
		}

		// 原始容量成倍扩容
		uint total = _Position + count;
		uint size = _Capacity;
		while(size < total) size <<= 1;

		// 申请新的空间，并复制数据
		byte* bufNew = new byte[size];
		if(_Position > 0) memcpy(bufNew, _Buffer, _Position);

		if(_Buffer != _Arr) delete[] _Buffer;

		_Buffer = bufNew;
		_Capacity = size;
	}

	return true;
}

uint Stream::ReadArray(ByteArray& bs)
{
	uint len = ReadEncodeInt();
	if(!len) return 0;

	if(len > bs.Capacity())
	{
		debug_printf("准备读取的数据长度是 %d，而缓冲区数组容量是 %d\r\n", len, bs.Capacity());
		//assert_param2(len <= bs.Capacity(), "缓冲区大小不足");
		//bs.Set(0, 0, len);
		// 即使缓冲区不够大，也不要随便去重置，否则会清空别人的数据
		// 这里在缓冲区不够大时，有多少读取多少
		len = bs.Capacity();
	}

	Read(bs.GetBuffer(), 0, len);

	return len;
}

bool Stream::WriteArray(const ByteArray& bs)
{
	WriteEncodeInt(bs.Length());
	return Write(bs.GetBuffer(), 0, bs.Length());
}

uint Stream::ReadString(String& str)
{
	ByteArray bs(str.Capacity() - str.Length());
	uint len = ReadArray(bs);
	if(!len) return 0;

	str.Copy((char*)bs.GetBuffer(), len, str.Length());

	return len;
}

bool Stream::WriteString(const String& str)
{
	ByteArray bs(str);
	return WriteArray(bs);
}
