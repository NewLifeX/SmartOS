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
	String& str = *(new String());
	byte* buf = GetBuffer();
	for(int i=0; i < Count(); i++, buf++)
	{
		//debug_printf("%02X", *buf++);
		byte b = *buf >> 4;
		str.Append(b > 9 ? ('A' + b - 10) : ('0' + b));
		b = *buf & 0x0F;
		str.Append(b > 9 ? ('A' + b - 10) : ('0' + b));
		
		if(i < Count() - 1)
		{
			if(newLine > 0 && (i + 1) % newLine == 0)
				str.Append("\r\n");
			else if(sep != '\0')
				str.Append(sep);
		}
	}
	return str;
}

/*String::String(int capacity)
{
	Init();

	if(capacity <= ArrayLength(Arr))
		_Arr = Arr;
	else
	{
		_Arr = new char[capacity];
		_needFree = true;
	}
}

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
	_Arr		= (char*)data;
}

String::String(String& str)
{
	Init();

	_Count		= str._Count;
	_Arr		= str._Arr;
	_needFree	= str._needFree;
}

// 重载等号运算符，使用另一个固定数组来初始化
String& String::operator=(String& str)
{
	_Count		= str._Count;
	_Arr		= str._Arr;
	_needFree	= str._needFree;

	return *this;
}

String::~String()
{
	if(_needFree)
	{
		if(_Arr) delete _Arr;
		_Arr = NULL;
	}
}

// 重载索引运算符[]，让它可以像数组一样使用下标索引。
char& String::operator[](int i)
{
	assert_param(i >= 0 && i < _Count);

	return _Arr[i];
}*/

// 输出对象的字符串表示方式
const char* String::ToString()
{
	return GetBuffer();
}

String& String::Append(char ch)
{
	Push(ch);

	return *this;
}

String& String::Append(const char* str)
{
	char* p = (char*)str;
	while(*p) Push(*p++);

	return *this;
}

// 调试输出字符串
void String::Show(bool newLine)
{
	debug_printf("%s", ToString());
	if(newLine) debug_printf("\r\n");
}
