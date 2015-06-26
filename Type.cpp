#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
#include "Sys.h"

String Object::ToString()
{
	const char* str = typeid(*this).name();
	while(*str >= '0' && *str <= '9') str++;

	return str;
}

void Object::Show()
{
	ToString().Print();
}

// 字符串转为字节数组
ByteArray::ByteArray(String& str) : Array(str.Length())
{
	Copy((byte*)str.ToString().GetBuffer(), str.Length());
}

// 显示十六进制数据，指定分隔字符
String ByteArray::ToHex(char sep, int newLine)
{
	byte* buf = GetBuffer();

	// 每个字节后面带一个横杠，有换行的时候两个字符，不带横杠
	int len = Length() * 2;
	if(sep != '\0') len += Length();
	if(newLine > 0) len += Length() / newLine;

	String str(len);
	char* ss = str.GetBuffer();
	int k = 0;
	for(int i=0; i < Length(); i++, buf++)
	{
		byte b = *buf >> 4;
		ss[k++] = b > 9 ? ('A' + b - 10) : ('0' + b);
		b = *buf & 0x0F;
		ss[k++] = b > 9 ? ('A' + b - 10) : ('0' + b);

		if(i < Length() - 1)
		{
			if(newLine > 0 && (i + 1) % newLine == 0)
			{
				ss[k++] = '\r';
				ss[k++] = '\n';
			}
			else if(sep != '\0')
				ss[k++] = sep;
		}
	}
	ss[k] = '\0';
	str.SetLength(k);

	return str;
}

String ByteArray::ToString()
{
	return ToHex('-', 0x20);
}

String& String::SetLength(int length)
{
	assert_param(length <= _Capacity);

	_Length = length;

	return *this;
}

// 输出对象的字符串表示方式
String String::ToString()
{
	return *this;
}

// 清空已存储数据。长度放大到最大容量
String& String::Clear()
{
	Array::Clear();

	_Length = 0;

	return *this;
}

String& String::Append(char ch)
{
	Copy(&ch, 1, Length());

	return *this;
}

String& String::Append(const char* str, int len)
{
	Copy(str, 0, Length());

	return *this;
}

char* _itoa(int value, char* string, int radix)
{
	char tmp[33];
	char* tp = tmp;
	int i;
	unsigned v;
	int sign;
	char* sp;
	if (radix > 36 || radix <= 1) return 0;

	sign = (radix == 10 && value < 0);
	if (sign)
		v = -value;
	else
		v = (unsigned)value;
	while (v || tp == tmp)
	{
		i = v % radix;
		v = v / radix;
		if (i < 10)
			*tp++ = i+'0';
		else
			*tp++ = i + 'A' - 10;
	}

	sp = string;
	if (sign)
		*sp++ = '-';
	while (tp > tmp)
		*sp++ = *--tp;
	*sp = 0;

	return string;
}

String& String::Append(int value)
{
	char ch[16];
	_itoa(value, ch, 10);

	return Append(ch);
}

String& String::Append(byte bt)
{
	int k = Length();
	SetLength(k + 2);

	byte b = bt >> 4;
	this[k++] = b > 9 ? ('A' + b - 10) : ('0' + b);
	b = bt & 0x0F;
	this[k++] = b > 9 ? ('A' + b - 10) : ('0' + b);

	return *this;
}

String& String::Append(ByteArray& bs)
{
	assert_param2(false, "未实现");
	bs.ToHex();

	return *this;
}

// 调试输出字符串
void String::Print(bool newLine)
{
	debug_printf("%s", GetBuffer());
	if(newLine) debug_printf("\r\n");
}

String& String::Format(const char* format, ...)
{
	va_list ap;

	//const char* fmt = format.GetBuffer();
	va_start(ap, format);

	int len = vsnprintf(GetBuffer(), Capacity(), format, ap);
	_Length = len;

	va_end(ap);

	return *this;
}

/* IP地址 */

IPAddress IPAddress::Any(0, 0, 0, 0);
IPAddress IPAddress::Broadcast(255, 255, 255, 255);

IPAddress::IPAddress(byte ip1, byte ip2, byte ip3, byte ip4)
{
	Value = (ip4 << 24) + (ip3 << 16) + (ip2 << 8) + ip1;
}

bool IPAddress::IsAny() { return Value == 0; }

bool IPAddress::IsBroadcast() { return Value == 0xFFFFFFFF; }

uint IPAddress::GetSubNet(IPAddress& mask)
{
	return Value & mask.Value;
}

// 重载索引运算符[]，让它可以像数组一样使用下标索引。
byte& IPAddress::operator[](int i)
{
	assert_param(i >= 0 && i < 4);

	return ((byte*)&Value)[i];
}

// 字节数组
byte* IPAddress::ToArray()
{
	return (byte*)&Value;
}

String IPAddress::ToString()
{
	byte* ips = (byte*)&Value;

	String str;
	str.Format("%d.%d.%d.%d", ips[0], ips[1], ips[2], ips[3]);

	return str;
}

void IPAddress::Show()
{
	byte* ips = (byte*)&Value;
	debug_printf("%d.%d.%d.%d", ips[0], ips[1], ips[2], ips[3]);
}

IPEndPoint::IPEndPoint()
{
	Address = IPAddress::Any;
	Port = 0;
}

IPEndPoint::IPEndPoint(IPAddress& addr, ushort port)
{
	Address = addr;
	Port	= port;
}

String IPEndPoint::ToString()
{
	String str = Address.ToString();

	char ss[7];
	int len = sprintf(ss, ":%d", Port);
	str.Copy(ss, len, str.Length());

	return str;
}

void IPEndPoint::Show()
{
	byte* ips = (byte*)&Address.Value;
	debug_printf("%d.%d.%d.%d:%d", ips[0], ips[1], ips[2], ips[3], Port);
}

bool operator==(IPEndPoint& addr1, IPEndPoint& addr2)
{
	return addr1.Port == addr2.Port && addr1.Address == addr2.Address;
}
bool operator!=(IPEndPoint& addr1, IPEndPoint& addr2)
{
	return addr1.Port != addr2.Port || addr1.Address != addr2.Address;
}

/* MAC地址 */

#define MAC_MASK 0xFFFFFFFFFFFFull

MacAddress MacAddress::Empty(0);
MacAddress MacAddress::Full(MAC_MASK);

MacAddress::MacAddress(ulong v)
{
	//v4 = v;
	//v2 = v >> 32;
	Value = v & MAC_MASK;
}

// 是否广播地址，全0或全1
bool MacAddress::IsBroadcast() { return Value == Empty.Value || Value == Full.Value; }

MacAddress& MacAddress::operator=(ulong v)
{
	//v4 = v;
	//v2 = v >> 32;

	// 下面这个写法很好，一条汇编指令即可完成，但是会覆盖当前结构体后两个字节
	//*(ulong*)this = v;

	// 下面的写法需要5条汇编指令，先放入内存，再分两次读写
	/*uint* p = (uint*)&v;
	v4 = *p++;
	v2 = *(ushort*)p;*/

	Value = v & MAC_MASK;

	return *this;
}

// 重载索引运算符[]，让它可以像数组一样使用下标索引。
byte& MacAddress::operator[](int i)
{
	assert_param(i >= 0 && i < 6);

	return ((byte*)&Value)[i];
}

// 字节数组
byte* MacAddress::ToArray()
{
	return (byte*)&Value;
}

/*bool MacAddress::operator==(MacAddress& addr1, MacAddress& addr2)
{
	return addr1.v4 == addr2.v4 && addr1.v2 == addr2.v2;
}

bool MacAddress::operator!=(MacAddress& addr1, MacAddress& addr2)
{
	return addr1.v4 != addr2.v4 || addr1.v2 != addr2.v2;
}*/

String MacAddress::ToString()
{
	String str;
	byte* macs = (byte*)&Value;

	str.Format("%02X-%02X-%02X-%02X-%02X-%02X", macs[0], macs[1], macs[2], macs[3], macs[4], macs[5]);

	return str;
}

void MacAddress::Show()
{
	byte* macs = (byte*)&Value;
	debug_printf("%02X-%02X-%02X-%02X-%02X-%02X", macs[0], macs[1], macs[2], macs[3], macs[4], macs[5]);
}
