//#include <string.h>

#include "_Core.h"

#include "Type.h"
#include "Buffer.h"
#include "Array.h"

/******************************** Array ********************************/

Array::Array(void* data, int len) : Buffer(data, len)
{
	Init();
}

Array::Array(const void* data, int len) : Buffer((void*)data, len)
{
	Init();

	_canWrite	= false;
}

Array::Array(const Buffer& rhs) : Buffer(nullptr, 0)
{
	Copy(0, rhs, 0, -1);

	Init();
}

Array::Array(Array&& rval) : Buffer(nullptr, 0)
{
	move(rval);
}

void Array::move(Array& rval)
{
	Buffer::move(rval);

	_Capacity	= rval._Capacity;
	_needFree	= rval._needFree;
	_canWrite	= rval._canWrite;
	Expand		= rval.Expand;

	rval._Capacity	= 0;
	rval._needFree	= false;
	rval._canWrite	= false;
}

void Array::Init()
{
	Expand	= true;
	_Size	= 1;

	_Capacity	= _Length;
	_needFree	= false;
	_canWrite	= true;
}

// 析构。释放资源
Array::~Array()
{
	Release();
}

// 释放已占用内存
bool Array::Release()
{
	auto p	= _Arr;
	bool fr	= _needFree;

	_Arr		= nullptr;
	_Capacity	= 0;
	_Length		= 0;
	_needFree	= false;
	_canWrite	= true;

	if(fr && p)
	{
		delete (byte*)p;

		return true;
	}

	return false;
}

Array& Array::operator = (const Buffer& rhs)
{
	// 可能需要先扩容，否则Buffer拷贝时，长度可能不准确
	// 长度也要相等，可能会因此而扩容
	//SetLength(rhs.Length());

	Buffer::operator=(rhs);

	return *this;
}

Array& Array::operator = (const void* p)
{
	Buffer::operator=(p);

	return *this;
}

Array& Array::operator = (Array&& rval)
{
	move(rval);

	return *this;
}

// 设置数组长度。容量足够则缩小Length，否则扩容以确保数组容量足够大避免多次分配内存
bool Array::SetLength(int len)
{
	return SetLength(len, false);
}

bool Array::SetLength(int len, bool bak)
{
	if(len <= _Capacity)
	{
		_Length = len;
	}
	else
	{
		if(!CheckCapacity(len, bak ? _Length : 0)) return false;
		// 扩大长度
		if(len > _Length) _Length = len;
	}
	return true;
}

/*void Array::SetBuffer(void* ptr, int len)
{
	Release();

	Buffer::SetBuffer(ptr, len);
}

void Array::SetBuffer(const void* ptr, int len)
{
	SetBuffer((void*)ptr, len);

	_canWrite	= false;
}*/

// 拷贝数据，默认-1长度表示使用右边最大长度，左边不足时自动扩容
int Array::Copy(int destIndex, const Buffer& src, int srcIndex, int len)
{
	// 可能需要先扩容，否则Buffer拷贝时，长度可能不准确
	int remain	= src.Length() - srcIndex;
	if(len < 0)
	{
		// -1时选择右边最大长度
		len	= remain;
		if(len <= 0) return 0;
	}
	else
	{
		// 右边可能不足len
		if(len > remain) len	= remain;
	}

	// 左边不足时自动扩容
	if(Length() < len) SetLength(len);

	return Buffer::Copy(destIndex, src, srcIndex, len);
}

// 设置数组元素为指定值，自动扩容
bool Array::SetItem(const void* data, int index, int count)
{
	assert(_canWrite, "禁止SetItem修改");
	assert(data, "Array::SetItem data Error");

	// count<=0 表示设置全部元素
	if(count <= 0) count = _Length - index;
	assert(count > 0, "Array::SetItem count Error");

	// 检查长度是否足够
	int len2 = index + count;
	CheckCapacity(len2, index);

	//byte* buf = (byte*)GetBuffer();
	// 如果元素类型大小为1，那么可以直接调用内存设置函数
	if(_Size == 1)
		//memset(&buf[index], *(byte*)data, count);
		Set(*(byte*)data, index, count);
	else
	{
		while(count-- > 0)
		{
			//memcpy(buf, data, _Size);
			//buf += _Size;
			Copy(index, data, _Size);
			index	+= _Size;
		}
	}

	// 扩大长度
	if(len2 > _Length) _Length = len2;

	return true;
}

// 设置数组。直接使用指针，不拷贝数据
bool Array::Set(void* data, int len)
{
	if(!Set((const void*)data, len)) return false;

	_canWrite	= true;

	return true;
}

// 设置数组。直接使用指针，不拷贝数据
bool Array::Set(const void* data, int len)
{
	// 销毁旧的
	if(_needFree && _Arr != data) Release();

	_Arr		= (char*)data;
	_Length		= len;
	_Capacity	= len;
	_needFree	= false;
	_canWrite	= false;

	return true;
}

// 清空已存储数据。
void Array::Clear()
{
	assert(_canWrite, "禁止Clear修改");
	assert(_Arr, "Clear数据不能为空指针");

	//memset(_Arr, 0, _Size * _Length);
	Set(0, 0, _Size * _Length);
}

// 设置指定位置的值，不足时自动扩容
void Array::SetItemAt(int i, const void* item)
{
	assert(_canWrite, "禁止SetItemAt修改");

	// 检查长度，不足时扩容
	CheckCapacity(i + 1, _Length);

	if(i >= _Length) _Length = i + 1;

	//memcpy((byte*)_Arr + _Size * i, item, _Size);
	Copy(_Size * i, item, _Size);
}

// 重载索引运算符[]，返回指定元素的第一个字节
byte Array::operator[](int i) const
{
	assert(_Arr && i >= 0 && i < _Length, "下标越界");

	byte* buf = (byte*)_Arr;
	if(_Size > 1) i *= _Size;

	return buf[i];
}

byte& Array::operator[](int i)
{
	assert(_Arr && i >= 0 && i < _Length, "下标越界");

	byte* buf = (byte*)_Arr;
	if(_Size > 1) i *= _Size;

	return buf[i];
}

// 检查容量。如果不足则扩大，并备份指定长度的数据
bool Array::CheckCapacity(int len, int bak)
{
	// 是否超出容量
	// 如果不是可写，在扩容检查时，也要进行扩容，避免内部不可写数据被修改
	if(_Arr && len <= _Capacity && _canWrite) return true;
	// 是否可以扩容
	if(!Expand) return false;

	// 自动计算合适的容量
	int sz = 0x40;
	while(sz < len) sz <<= 1;

	bool _free	= _needFree;

	void* p = Alloc(sz);
	if(!p) return false;

	// 是否需要备份数据
	if(bak > _Length) bak = _Length;
	if(bak > 0 && _Arr)
		// 为了安全，按照字节拷贝
		Buffer(p, sz).Copy(0, _Arr, bak);

	int oldlen	= _Length;
	if(_free && _Arr != p) Release();

	_Arr		= (char*)p;
	_Capacity	= sz;
	_Length		= oldlen;

	// _needFree 由Alloc决定
	// 有可能当前用的内存不是内部内存，然后要分配的内存小于内部内存，则直接使用内部，不需要释放
	//_needFree	= true;

	return true;
}

void* Array::Alloc(int len)
{
	_needFree	= true;

	return new byte[_Size * len];
}

bool operator==(const Array& bs1, const Array& bs2)
{
	if(bs1.Length() != bs2.Length()) return false;

	return bs1.CompareTo(bs2) == 0;
	//return memcmp(bs1._Arr, bs2._Arr, bs1.Length() * bs1._Size) == 0;
}

bool operator!=(const Array& bs1, const Array& bs2)
{
	if(bs1.Length() != bs2.Length()) return true;

	return bs1.CompareTo(bs2) != 0;
	//return memcmp(bs1._Arr, bs2._Arr, bs1.Length() * bs1._Size) != 0;
}
