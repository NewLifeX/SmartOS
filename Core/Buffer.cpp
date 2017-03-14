#if defined(__CC_ARM)
#include <string.h>
#else
#include <cstring>
#endif

#include "_Core.h"

#include "Buffer.h"
#include "SString.h"

/******************************** Buffer ********************************/

Buffer::Buffer(void* ptr, int len)
{
	_Arr = (char*)ptr;
	_Length = len;
}

/*Buffer::Buffer(const Buffer& buf)
{
	Copy(0, rhs, 0, -1);
}*/

Buffer::Buffer(Buffer&& rval)
{
	move(rval);
}

void Buffer::move(Buffer& rval)
{
	_Arr = rval._Arr;
	_Length = rval._Length;

	rval._Arr = nullptr;
	rval._Length = 0;
}

Buffer& Buffer::operator = (const Buffer& rhs)
{
	if (!SetLength(rhs.Length())) assert(false, "赋值操作无法扩容");

	Copy(rhs, 0);

	return *this;
}

Buffer& Buffer::operator = (const void* ptr)
{
	if (ptr) Copy(0, ptr, -1);

	return *this;
}

Buffer& Buffer::operator = (Buffer&& rval)
{
	move(rval);

	return *this;
}

// 设置指定位置的值，长度不足时自动扩容
bool Buffer::SetAt(int index, byte value)
{
	if (index >= _Length && !SetLength(index + 1)) return false;

	_Arr[index] = value;

	return true;
}

// 重载索引运算符[]，返回指定元素的第一个字节
byte Buffer::operator[](int i) const
{
	assert(i >= 0 && i < _Length, "下标越界");

	byte* buf = (byte*)_Arr;

	return buf[i];
}

byte& Buffer::operator[](int i)
{
	assert(i >= 0 && i < _Length, "下标越界");

	byte* buf = (byte*)_Arr;

	return buf[i];
}

// 设置数组长度。容量足够则缩小Length，否则失败。子类可以扩展以实现自动扩容
bool Buffer::SetLength(int len)
{
	if (len > _Length) return false;

	_Length = len;

	return true;
}

/*void Buffer::SetBuffer(void* ptr, int len)
{
	_Arr		= (char*)ptr;
	_Length		= len;
}*/

// 原始拷贝、清零，不检查边界
void Buffer::Copy(void* dest, const void* src, int len)
{
	memmove(dest, src, len);
}

void Buffer::Zero(void* dest, int len)
{
	memset(dest, 0, len);
}

// 拷贝数据，默认-1长度表示当前长度
int Buffer::Copy(int destIndex, const void* src, int len)
{
	if (!src) return 0;

	// 自己有缓冲区才能拷贝
	if (!_Arr || !_Length) return 0;

	int remain = _Length - destIndex;

	// 如果没有指明长度，则拷贝起始位置之后的剩余部分
	if (len < 0)
	{
		// 不指定长度，又没有剩余量，无法拷贝
		if (remain <= 0)
		{
			debug_printf("Buffer::Copy (0x%p, %d) <= (%d, 0x%p, %d) \r\n", _Arr, _Length, destIndex, src, len);
			assert(false, "Buffer::Copy 未指明要拷贝的长度");

			return 0;
		}

		len = remain;
	}
	else if (len > remain)
	{
		// 要拷贝进来的数据超过内存大小，给子类尝试扩容，如果扩容失败，则只拷贝没有超长的那一部分
		// 子类可能在这里扩容
		if (!SetLength(destIndex + len))
		{
			debug_printf("Buffer::Copy (0x%p, %d) <= (%d, 0x%p, %d) \r\n", _Arr, _Length, destIndex, src, len);
			assert(false, "Buffer::Copy 缓冲区太小");

			len = remain;
		}
	}

	// 放到这里判断，前面有可能自动扩容
	if (!_Arr) return 0;

	// 自我拷贝，跳过
	if (_Arr + destIndex == src) return len;

	// 拷贝数据
	if (len) memmove((byte*)_Arr + destIndex, (byte*)src, len);

	return len;
}

// 拷贝数据，默认-1长度表示两者最小长度
int Buffer::Copy(int destIndex, const Buffer& src, int srcIndex, int len)
{
	if (len < 0) len = _Length - destIndex;

	// 允许自身拷贝
	// 源数据的实际长度可能跟要拷贝的长度不一致
	int remain = src._Length - srcIndex;
	if (len > remain) len = remain;
	//if(len <= 0) return 0;

	return Copy(destIndex, (byte*)src._Arr + srcIndex, len);
}

int Buffer::Copy(const Buffer& src, int destIndex)
{
	return Copy(destIndex, (byte*)src._Arr, src.Length());
}

// 把数据复制到目标缓冲区，默认-1长度表示当前长度
int Buffer::CopyTo(int srcIndex, void* data, int len) const
{
	if (!_Arr || !data) return 0;

	int remain = _Length - srcIndex;
	if (remain <= 0) return 0;

	if (len < 0 || len > remain) len = remain;

	// 拷贝数据
	if (len)
	{
		if (data != (byte*)_Arr + srcIndex)
			memmove((byte*)data, (byte*)_Arr + srcIndex, len);
	}

	return len;
}

// 用指定字节设置初始化一个区域
int Buffer::Set(byte item, int index, int len)
{
	if (!_Arr || len == 0) return 0;

	int remain = _Length - index;
	if (remain <= 0) return 0;

	if (len < 0 || len > remain) len = remain;

	if (len) memset((byte*)_Arr + index, item, len);

	return len;
}

void Buffer::Clear(byte item)
{
	Set(item, 0, _Length);
}

// 截取一个子缓冲区，默认-1长度表示剩余全部
//### 这里逻辑可以考虑修改为，当len大于内部长度时，直接用内部长度而不报错，方便应用层免去比较长度的啰嗦
Buffer Buffer::Sub(int index, int len)
{
	assert(index >= 0, "index >= 0");
	assert(index < _Length, "index < _Length");
	if (len < 0) len = _Length - index;
	assert(index + len <= _Length, "len <= _Length");

	return Buffer((byte*)_Arr + index, len);
}

const Buffer Buffer::Sub(int index, int len) const
{
	assert(index >= 0, "index >= 0");
	if (len < 0) len = _Length - index;
#if DEBUG
	if (index + len > _Length) debug_printf("Buffer::Sub (%d, %d) > %d \r\n", index, len, _Length);
#endif
	//assert(index <= _Length, "index < _Length");
	assert(index + len <= _Length, "index + len <= _Length");

	return Buffer((byte*)_Arr + index, len);
}

// 显示十六进制数据，指定分隔字符
String& Buffer::ToHex(String& str, char sep, int newLine) const
{
	auto buf = GetBuffer();

	// 拼接在字符串后面
	for (int i = 0; i < Length(); i++, buf++)
	{
		if (i)
		{
			if (newLine > 0 && i % newLine == 0)
				str += "\r\n";
			else if (sep != '\0')
				str += sep;
		}

		str.Concat(*buf, -16);
	}

	return str;
}

// 显示十六进制数据，指定分隔字符
String Buffer::ToHex(char sep, int newLine) const
{
	String str;

	// 优化为使用RVO
	ToHex(str, sep, newLine);

	return str;
}

ushort	Buffer::ToUInt16() const
{
	auto p = GetBuffer();
	// 字节对齐时才能之前转为目标整数
	if (((int)p & 0x01) == 0) return *(ushort*)p;

	return p[0] | (p[1] << 8);
}

uint	Buffer::ToUInt32() const
{
	auto p = GetBuffer();
	// 字节对齐时才能之前转为目标整数
	if (((int)p & 0x03) == 0) return *(uint*)p;

	return p[0] | (p[1] << 8) | (p[2] << 0x10) | (p[3] << 0x18);
}

UInt64	Buffer::ToUInt64() const
{
	auto p = GetBuffer();
	// 字节对齐时才能之前转为目标整数
	if (((int)p & 0x07) == 0) return *(UInt64*)p;

	uint n1 = p[0] | (p[1] << 8) | (p[2] << 0x10) | (p[3] << 0x18);
	p += 4;
	uint n2 = p[0] | (p[1] << 8) | (p[2] << 0x10) | (p[3] << 0x18);

	return n1 | ((UInt64)n2 << 0x20);
}

void Buffer::Write(ushort value, int index)
{
	Copy(index, (byte*)&value, sizeof(ushort));
}

void Buffer::Write(short value, int index)
{
	Copy(index, (byte*)&value, sizeof(short));
}

void Buffer::Write(uint value, int index)
{
	Copy(index, (byte*)&value, sizeof(uint));
}

void Buffer::Write(int value, int index)
{
	Copy(index, (byte*)&value, sizeof(int));
}

void Buffer::Write(UInt64 value, int index)
{
	Copy(index, (byte*)&value, sizeof(UInt64));
}

String& Buffer::ToStr(String& str) const
{
	return ToHex(str, '-', 0x20);
}

// 包装为字符串对象
String Buffer::AsString() const
{
	String str((cstring)_Arr, _Length);
	return str;
}

int Buffer::CompareTo(const Buffer& bs) const
{
	return CompareTo(bs._Arr, bs._Length);
}

int Buffer::CompareTo(const void* ptr, int len) const
{
	if (len < 0) len = _Length;

	// 同一个指针，长度决定大小
	if (_Arr == ptr) return _Length - len;

	// 以最短长度来比较
	int count = _Length;
	if (count > len)	count = len;

	// 遍历每一个字符
	for (cstring p1 = _Arr, p2 = (cstring)ptr; count > 0; count--, p1++, p2++)
	{
		int n = *p1 - *p2;
		if (n) return n;
	}

	// 判断剩余长度，以此决定大小
	return _Length - len;
}

bool operator == (const Buffer& bs1, const Buffer& bs2)
{
	if (bs1._Arr == bs2._Arr) return true;
	if (!bs1._Arr || !bs2._Arr) return false;
	if (bs1.Length() != bs2.Length()) return false;

	return bs1.CompareTo(bs2) == 0;
}

bool operator == (const Buffer& bs1, const void* ptr)
{
	if (bs1._Arr == ptr) return true;
	if (!bs1._Arr || !ptr) return false;

	return bs1.CompareTo(ptr) == 0;
}

bool operator != (const Buffer& bs1, const Buffer& bs2)
{
	if (bs1._Arr == bs2._Arr) return false;
	if (!bs1._Arr || !bs2._Arr) return true;
	if (bs1.Length() != bs2.Length()) return true;

	return bs1.CompareTo(bs2) != 0;
}

bool operator != (const Buffer& bs1, const void* ptr)
{
	if (bs1._Arr == ptr) return false;
	if (!bs1._Arr || !ptr) return true;

	return bs1.CompareTo(ptr) != 0;
}

/******************************** BufferRef ********************************/

// 打包一个指针和长度指定的数据区
void BufferRef::Set(void* ptr, int len)
{
	_Arr = (char*)ptr;
	_Length = len;
}
