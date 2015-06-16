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

ByteArray::ByteArray(String& str) : Array((byte*)str.ToString(), str.Count()) { }

// 显示十六进制数据，指定分隔字符
String& ByteArray::ToHex(char sep, int newLine)
{
	String* str = new String();
	byte* buf = GetBuffer();
	for(int i=0; i < Count(); i++)
	{
		debug_printf("%02X", *buf++);
		if(newLine > 0 && i%newLine == 0)
			debug_printf("\r\n");
		else if(i < Count() - 1 && sep != '\0')
			debug_printf("%c", sep);
	}
	return *str;
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

	// 输出对象的字符串表示方式
const char* String::ToString()
{
	return _Arr;
}

// 调试输出字符串
void String::Show(bool newLine)
{
	debug_printf("%s", _Arr);
	if(newLine) debug_printf("\r\n");
}
