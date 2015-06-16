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

	_Capacity = len;
	_Position = 0;
	Length = 0;
}

// 使用缓冲区初始化数据流。注意，此时指针位于0，而内容长度为缓冲区长度
Stream::Stream(byte* buf, uint len)
{
	assert_ptr(buf);
	assert_param(len > 0);

	_Buffer = buf;
	_Capacity = len;
	_Position = 0;
	_needFree = false;
	Length = len;
}

// 销毁数据流
Stream::~Stream()
{
	assert_ptr(this);
	if(_needFree)
	{
		if(_Buffer) delete[] _Buffer;
		_Buffer = NULL;
	}
}

// 数据流容量
uint Stream::Capacity() { return _Capacity; }

// 当前位置
uint Stream::Position() { return _Position; }

// 设置位置
void Stream::SetPosition(uint p)
{
	// 允许移动到最后一个字节之后，也就是Length
	assert_param(p <= Length);

	_Position = p;
}

// 余下的有效数据流长度。0表示已经到达终点
uint Stream::Remain() { return Length - _Position; };

// 尝试前后移动一段距离，返回成功或者失败。如果失败，不移动游标
bool Stream::Seek(int offset)
{
	if(offset == 0) return true;

	int p = offset + _Position;
	//if(p < 0 || p >= Length) return false;
	// 允许移动到最后一个字节之后，也就是Length
	if(p < 0 || p > Length) return false;

	_Position = p;

	return true;
}

// 数据流指针。注意：扩容后指针会改变！
byte* Stream::GetBuffer() { return _Buffer; }

// 数据流当前位置指针。注意：扩容后指针会改变！
byte* Stream::Current() { return &_Buffer[_Position]; }

// 从当前位置读取数据
uint Stream::Read(byte* buf, uint offset, int count)
{
	assert_param(buf);

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
	uint temp = 0;
	for(int i = 0; i < 4; i++)
	{
		temp = (uint)ReadBytes(1);
		if(temp<127)
		{
			value |= ( temp << (7*i));
			return value;
		}
		temp &= 0x7f;
		value |= ( temp << (7*i));
	}
	return 0xffffffff;
}

// 把数据写入当前位置
void Stream::Write(byte* buf, uint offset, uint count)
{
	assert_param(buf);

	if(!CheckCapacity(count)) return;

	memcpy(Current(), buf + offset, count);

	_Position += count;
	//Length += count;
	// 内容长度不是累加，而是根据位置而扩大
	if(_Position > Length) Length = _Position;
}

uint Stream::WriteEncodeInt(uint value)
{
	byte temp;
	for( int i = 0 ; i < 4 ; i++ )
	{
		temp = (byte)value;
		if(temp < 127)
		{
			Write(&temp, 0, 1);
			return i+1;
		}
		temp |= 0x80;
		Write(&temp, 0, 1);
		value>>=7;
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
	Write((byte*)str, 0, len);

	return len;
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
int Stream::Peek()
{
	if(!Remain()) return -1;
	return *Current();
}

bool Stream::CheckCapacity(uint count)
{
	uint remain = _Capacity - _Position;
	// 容量不够，需要扩容
	if(count > remain)
	{
		if(!_needFree)
		{
			debug_printf("数据流剩余容量%d不足%d，而外部缓冲区无法扩容！", remain, count);
			assert_param(false);
			return false;
		}

		// 原始容量成倍扩容
		uint total = _Position + count;
		uint size = _Capacity;
		while(size < total) size <<= 1;

		// 申请新的空间，并复制数据
		byte* bufNew = new byte[size];
		if(_Position > 0) memcpy(bufNew, _Buffer, _Position);

		delete[] _Buffer;

		_Buffer = bufNew;
		_Capacity = size;
	}

	return true;
}
