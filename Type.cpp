#include "Sys.h"

Object::Object(int size)
{
	// 清空整个对象。跳过最前面的4字节虚表
	byte* p = (byte*)this;
	p += 4;
	memset((void*)p, 0, size - 4);

	_Size = size;
}

const char* Object::ToString()
{
	const char* str = typeid(*this).name();
	while(*str >= '0' && *str <= '9') str++;

	return str;
}

String::String(const char* data, int len) : Object(sizeof(this[0]))
{
	// 自动计算长度，\0结尾
	if(!len)
	{
		char* p = (char*)data;
		while(*p++) len++;
	}

	_Count		= len;
	//_Capacity	= len;
	//_Arr		= data;
	_Arr = new char[len];
	memcpy(_Arr, data, len);
}

String::String(String& str) : Object(sizeof(this[0]))
{
	_Count		= str._Count;
	//_Capacity	= str._Capacity;
	memcpy(_Arr, str._Arr, _Count);
}

// 重载等号运算符，使用另一个固定数组来初始化
String& String::operator=(String& str)
{
	_Count		= str._Count;
	//_Capacity	= str._Capacity;
	memcpy(_Arr, str._Arr, _Count);

	return *this;
}

String::~String()
{
	if(_Arr) delete _Arr;
	_Arr = NULL;
}

// 重载索引运算符[]，让它可以像数组一样使用下标索引。
char String::operator[](int i)
{
	assert_param(i >= 0 && i < _Count);

	return _Arr[i];
}
