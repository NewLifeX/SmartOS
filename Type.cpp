#include "Sys.h"

void Object::Init(int size)
{
	// 清空整个对象。跳过最前面的4字节虚表
	byte* p = (byte*)this;
	p += 4;
	memset((void*)p, 0, size - 4);
}

const char* Object::ToString()
{
	const char* str = typeid(*this).name();
	while(*str >= '0' && *str <= '9') str++;

	return str;
}

String::String() { Init(); }

String::String(const char* data, int len)
{
	Init();

	// 自动计算长度，\0结尾
	if(!len)
	{
		char* p = (char*)data;
		while(*p++) len++;
	}

	_Count		= len;
	_Arr		= data;
}

String::String(String& str)
{
	Init();

	_Count		= str._Count;
	_Arr		= str._Arr;
}

// 重载等号运算符，使用另一个固定数组来初始化
String& String::operator=(String& str)
{
	_Count		= str._Count;
	_Arr		= str._Arr;

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
