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

// 字符串转为字节数组
ByteArray::ByteArray(String& str) : Array(str.Length())
{
	Set((byte*)str.ToString(), str.Length());
}

// 显示十六进制数据，指定分隔字符
String& ByteArray::ToHex(String& str, char sep, int newLine)
{
	//String& str = *(new String());
	str.Clear();
	byte* buf = GetBuffer();
	for(int i=0, k=0; i < Length(); i++, buf++)
	{
		//debug_printf("%02X", *buf++);
		byte b = *buf >> 4;
		str[k++] = b > 9 ? ('A' + b - 10) : ('0' + b);
		b = *buf & 0x0F;
		str[k++] = b > 9 ? ('A' + b - 10) : ('0' + b);
		
		if(i < Length() - 1)
		{
			if(newLine > 0 && (i + 1) % newLine == 0)
			{
				str[k++] = '\r';
				str[k++] = '\n';
			}
			else if(sep != '\0')
				str[k++] = sep;
		}
	}
	return str;
}

// 输出对象的字符串表示方式
const char* String::ToString()
{
	return GetBuffer();
}

/*String& String::Append(char ch)
{
	Push(ch);

	return *this;
}

String& String::Append(const char* str)
{
	char* p = (char*)str;
	while(*p) Push(*p++);

	return *this;
}*/

// 调试输出字符串
void String::Show(bool newLine)
{
	debug_printf("%s", ToString());
	if(newLine) debug_printf("\r\n");
}
