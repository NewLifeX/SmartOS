#include "_Core.h"

#include "ByteArray.h"

/******************************** ByteArray ********************************/

ByteArray::ByteArray(int length) : Array(Arr, sizeof(Arr))
{
	//_Length	= length;
	SetLength(length);
}

ByteArray::ByteArray(byte item, int length) : Array(Arr, sizeof(Arr))
{
	//_Length	= length;
	SetLength(length);
	Set(item, 0, length);
}

ByteArray::ByteArray(const void* data, int length, bool copy) : Array(Arr, sizeof(Arr))
{
	if(copy)
	{
		_Length	=	length;
		Copy(0, data, length);
	}
	else
		Set(data, length);
}

ByteArray::ByteArray(void* data, int length, bool copy) : Array(Arr, sizeof(Arr))
{
	if(copy)
	{
		_Length	=	length;
		Copy(0, data, length);
	}
	else
		Set(data, length);
}

ByteArray::ByteArray(const Buffer& arr) : Array(Arr, arr.Length())
{
	Copy(0, arr, 0, -1);
}

/*ByteArray::ByteArray(const ByteArray& arr) : Array(Arr, arr.Length())
{
	Copy(0, arr, 0, -1);
}*/

ByteArray::ByteArray(ByteArray&& rval) : Array((const void*)nullptr, 0)
{
	move(rval);
}

/*// 字符串转为字节数组
ByteArray::ByteArray(String& str) : Array(Arr, str.Length())
{
	auto p = str.GetBuffer();
	Set((byte*)p, str.Length());
}

// 不允许修改，拷贝
ByteArray::ByteArray(const String& str) : Array(Arr, str.Length())
{
	cstring p = str.GetBuffer();
	//Copy((const byte*)p, str.Length());
	Set((const byte*)p, str.Length());
}*/

void ByteArray::move(ByteArray& rval)
{
	/*
	move逻辑：
	1，如果右值是内部指针，则必须拷贝数据，因为右值销毁的时候，内部数据跟着释放
	2，如果右值是外部指针，并且需要释放，则直接拿指针过来使用，由当前对象负责释放
	3，如果右值是外部指针，而不需要释放，则拷贝数据，因为那指针可能是借用外部的栈内存
	*/

	if(rval._Arr != (char*)rval.Arr && rval._needFree)
	{
		Array::move(rval);

		return;
	}

	SetLength(rval.Length());
	Copy(0, rval._Arr, rval._Length);
}

void* ByteArray::Alloc(int len)
{
	if(len <= (int)sizeof(Arr))
	{
		_needFree	= false;
		return Arr;
	}
	else
	{
		_needFree	= true;
		return new byte[len];
	}
}

ByteArray& ByteArray::operator = (const Buffer& rhs)
{
	Buffer::operator=(rhs);

	return *this;
}

ByteArray& ByteArray::operator = (const ByteArray& rhs)
{
	Buffer::operator=(rhs);

	return *this;
}

ByteArray& ByteArray::operator = (const void* p)
{
	Buffer::operator=(p);

	return *this;
}

ByteArray& ByteArray::operator = (ByteArray&& rval)
{
	move(rval);

	return *this;
}

// 保存到普通字节数组，首字节为长度
int ByteArray::Load(const void* data, int maxsize)
{
	const byte* p = (const byte*)data;
	_Length = p[0] <= maxsize ? p[0] : maxsize;

	return Copy(0, p + 1, _Length);
}

// 从普通字节数据组加载，首字节为长度
int ByteArray::Save(void* data, int maxsize) const
{
	assert(_Length <= 0xFF, "_Length <= 0xFF");

	byte* p = (byte*)data;
	int len = _Length <= maxsize ? _Length : maxsize;
	p[0] = len;

	return CopyTo(0, p + 1, len);
}
