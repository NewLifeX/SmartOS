#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
#include "Sys.h"

/******************************** Object ********************************/

// 输出对象的字符串表示方式
String& Object::ToStr(String& str) const
{
	const char* name = typeid(*this).name();
	while(*name >= '0' && *name <= '9') name++;

	str.Set(name);

	return str;
}

// 输出对象的字符串表示方式。支持RVO优化。
// 该方法直接返回给另一个String作为初始值，只有一次构造，没有多余构造、拷贝和析构。
String Object::ToString() const
{
	String str;
	ToStr(str);

	return str;
}

void Object::Show(bool newLine) const
{
	//String str;
	// 为了减少堆分配，采用较大的栈缓冲区
	char cs[0x200];
	String str(cs, ArrayLength(cs));
	str.SetLength(0);
	ToStr(str);
	str.Show(newLine);
}

/*Type Object::GetType() const
{
	return Type(&typeid(*this));
}*/

/******************************** Type ********************************/

/*Type::Type(type_info* ti)
{
	_info = ti;

	const char* name = typeid(*this).name();
	while(*name >= '0' && *name <= '9') name++;

	Name.Set(name);
}*/

/******************************** ByteArray ********************************/

ByteArray::ByteArray(const byte* data, int length, bool copy) : Array(0)
{
	if(copy)
		Copy(data, length);
	else
		Set(data, length);
}

// 字符串转为字节数组
ByteArray::ByteArray(String& str) : Array(0)
{
	char* p = str.GetBuffer();
	Set((byte*)p, str.Length());
}

// 不允许修改，拷贝
ByteArray::ByteArray(const String& str) : Array(0)
{
	char* p = ((String&)str).GetBuffer();
	Copy((byte*)p, str.Length());
}

// 重载等号运算符，使用外部指针、内部长度，用户自己注意安全
ByteArray& ByteArray::operator=(const byte* data)
{
	Set(data, Length());

	return *this;
}

// 显示十六进制数据，指定分隔字符
String& ByteArray::ToHex(String& str, char sep, int newLine) const
{
	byte* buf = GetBuffer();

	// 拼接在字符串后面
	for(int i=0; i < Length(); i++, buf++)
	{
		str.Append(*buf);

		if(i < Length() - 1)
		{
			if(newLine > 0 && (i + 1) % newLine == 0)
				str += "\r\n";
			else if(sep != '\0')
				str.Append(sep);
		}
	}

	return str;
}

// 显示十六进制数据，指定分隔字符
String ByteArray::ToHex(char sep, int newLine) const
{
	String str;

	return ToHex(str, sep, newLine);
}

String& ByteArray::ToStr(String& str) const
{
	return ToHex(str, '-', 0x20);
}

// 显示对象。默认显示ToString
void ByteArray::Show(bool newLine) const
{
	/*// 每个字节后面带一个横杠，有换行的时候两个字符，不带横杠
	int len = Length() * 2;
	if(sep != '\0') len += Length();
	if(newLine > 0) len += Length() / newLine;*/

	// 采用栈分配然后复制，避免堆分配
	char cs[512];
	String str(cs, ArrayLength(cs));
	// 清空字符串，变成0长度，因为ToHex内部是附加
	str.Clear();

	// 如果需要的缓冲区超过512，那么让它自己分配好了
	ToHex(str, '-', 0x20);

	str.Show(newLine);
}

ushort	ByteArray::ToUInt16() const
{
	byte* p = GetBuffer();
	// 字节对齐时才能之前转为目标整数
	if(((int)p & 0x01) == 0) return *(ushort*)p;

	return p[0] | (p[1] << 8);
}

uint	ByteArray::ToUInt32() const
{
	byte* p = GetBuffer();
	// 字节对齐时才能之前转为目标整数
	if(((int)p & 0x03) == 0) return *(uint*)p;

	return p[0] | (p[1] << 8) | (p[2] << 0x10) | (p[3] << 0x18);
}

ulong	ByteArray::ToUInt64() const
{
	byte* p = GetBuffer();
	// 字节对齐时才能之前转为目标整数
	if(((int)p & 0x07) == 0) return *(ulong*)p;

	uint n1 = p[0] | (p[1] << 8) | (p[2] << 0x10) | (p[3] << 0x18);
	p += 4;
	uint n2 = p[0] | (p[1] << 8) | (p[2] << 0x10) | (p[3] << 0x18);

	return n1 | ((ulong)n2 << 0x20);
}

void ByteArray::Write(ushort value, int index)
{
	Copy((byte*)&value, sizeof(ushort), index);
}

void ByteArray::Write(short value, int index)
{
	Copy((byte*)&value, sizeof(short), index);
}

void ByteArray::Write(uint value, int index)
{
	Copy((byte*)&value, sizeof(uint), index);
}

void ByteArray::Write(int value, int index)
{
	Copy((byte*)&value, sizeof(int), index);
}

void ByteArray::Write(ulong value, int index)
{
	Copy((byte*)&value, sizeof(ulong), index);
}

/******************************** String ********************************/

// 输出对象的字符串表示方式
String& String::ToStr(String& str) const
{
	// 把当前字符串复制到目标字符串后面
	str.Copy(*this, str.Length());

	return (String&)*this;
}

// 输出对象的字符串表示方式
String String::ToString() const
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

char* _itoa(int value, char* str, int radix, int width = 0)
{
	if (radix > 36 || radix <= 1) return 0;

	// 先处理符号
	uint v;
	bool sign = (radix == 10 && value < 0);
	if (sign)
		v = -value;
	else
		v = (uint)value;

	// 从低位数字开始，转为字符以后写入临时字符数组
	char tmp[33];
	char* tp = tmp;
	while (v || tp == tmp)
	{
		int i = v % radix;
		v = v / radix;
		if (i < 10)
			*tp++ = i + '0';
		else
			*tp++ = i + 'A' - 10;
	}

	char* sp = str;
	// 修正宽度
	if(width > 0)
	{
		width -= tp - tmp;
		if (sign) width--;
		while(width-- > 0) *sp++ = '0';
	}

	// 写入符号
	if (sign) *sp++ = '-';

	// 倒序写入目标字符串
	while (tp > tmp)
		*sp++ = *--tp;
	*sp = 0;

	return str;
}

// 写入整数，第二参数指定宽带，不足时补零
String& String::Append(int value, int radix, int width)
{
	char ch[16];
	_itoa(value, ch, radix, width);

	return Append(ch);
}

String& String::Append(byte bt)
{
	int k = Length();

	byte b = bt >> 4;
	SetAt(k++, b > 9 ? ('A' + b - 10) : ('0' + b));
	b = bt & 0x0F;
	SetAt(k++, b > 9 ? ('A' + b - 10) : ('0' + b));

	return *this;
}

String& String::Append(ByteArray& bs)
{
	bs.ToHex(*this);

	return *this;
}

// 调试输出字符串
void String::Show(bool newLine) const
{
	int len = Length();
	if(!len) return;

	// C格式字符串以0结尾
	char* p = GetBuffer();
	if(p[len] != 0 && !IN_ROM_SECTION(p)) p[len] = 0;

	debug_printf("%s", p);
	if(newLine) debug_printf("\r\n");
}

// 格式化字符串，输出到现有字符串后面。方便我们连续格式化多个字符串
String& String::Format(const char* format, ...)
{
	va_list ap;

	//const char* fmt = format.GetBuffer();
	va_start(ap, format);

	// 无法准确估计长度，大概乘以2处理
	int len = Length();
	CheckCapacity(len + (strlen(format) << 1), len);

	char* p = GetBuffer();
	len = vsnprintf(p + len, Capacity() - len, format, ap);
	_Length += len;

	va_end(ap);

	return *this;
}

String& String::Concat(const Object& obj)
{
	return obj.ToStr(*this);
}

String& String::Concat(const char* str, int len)
{
	Copy(str, 0, Length());

	return *this;
}

String& String::operator+=(const Object& obj)
{
	return this->Concat(obj);
}

String& String::operator+=(const char* str)
{
	return this->Concat(str);
}

String& operator+(String& str1, const Object& obj)
{
	return str1.Concat(obj);
}

String& operator+(String& str1, const char* str2)
{
	return str1.Concat(str2);
}

/*String operator+(const char* str1, const char* str2)
{
	String str(str1);
	return str + str2;
}*/

String operator+(const char* str, const Object& obj)
{
	String s;
	s = str;
	s += obj;
	return s;
}

String operator+(const Object& obj, const char* str)
{
	String s;
	obj.ToStr(s);
	s += str;
	return s;
}

String& operator+(String& str, char ch) { return str.Append(ch); }
String& operator+(String& str, byte bt) { return str.Append(bt); }
String& operator+(String& str, int value) { return str.Append(value); }

/******************************** IPAddress ********************************/
/* IP地址 */

//const IPAddress IPAddress::Any(0, 0, 0, 0);
//const IPAddress IPAddress::Broadcast(255, 255, 255, 255);

IPAddress::IPAddress(const byte* ips)
{
	memcpy((byte*)&Value, ips, 4);
}

IPAddress::IPAddress(byte ip1, byte ip2, byte ip3, byte ip4)
{
	Value = (ip4 << 24) + (ip3 << 16) + (ip2 << 8) + ip1;
}

IPAddress::IPAddress(const ByteArray& arr)
{
	memcpy((byte*)&Value, arr.GetBuffer(), 4);
}

bool IPAddress::IsAny() const { return Value == 0; }

bool IPAddress::IsBroadcast() const { return Value == 0xFFFFFFFF; }

uint IPAddress::GetSubNet(const IPAddress& mask) const
{
	return Value & mask.Value;
}

const IPAddress& IPAddress::Any()
{
	static const IPAddress _Any(0, 0, 0, 0);
	return _Any;
}

const IPAddress& IPAddress::Broadcast()
{
	static const IPAddress _Broadcast(255, 255, 255, 255);
	return _Broadcast;
}

IPAddress& IPAddress::operator=(const byte* v)
{
	//Value = *(uint*)v;
	memcpy((byte*)&Value, v, 4);

	return *this;
}

// 重载索引运算符[]，让它可以像数组一样使用下标索引。
byte& IPAddress::operator[](int i)
{
	assert_param(i >= 0 && i < 4);

	return ((byte*)&Value)[i];
}

// 字节数组
ByteArray IPAddress::ToArray() const
{
	//return ByteArray((byte*)&Value, 4);

	// 要复制数据，而不是直接使用指针，那样会导致外部修改内部数据
	return ByteArray((byte*)&Value, 4, true);
}

String& IPAddress::ToStr(String& str) const
{
	byte* ips = (byte*)&Value;

	for(int i=0; i<4; i++)
	{
		if(i > 0) str.Append('.');
		str.Append((int)ips[i]);
	}

	return str;
}

/******************************** IPEndPoint ********************************/

//const IPEndPoint IPEndPoint::Any(IPAddress::Any, 0);

IPEndPoint::IPEndPoint()
{
	Address = 0;
	Port = 0;
}

IPEndPoint::IPEndPoint(const IPAddress& addr, ushort port)
{
	Address = addr;
	Port	= port;
}

IPEndPoint::IPEndPoint(const ByteArray& arr)
{
	byte* p = arr.GetBuffer();
	Address = p;
	Port	= *(ushort*)(p + 4);
}

String& IPEndPoint::ToStr(String& str) const
{
	Address.ToStr(str);

	//char ss[7];
	//int len = sprintf(ss, ":%d", Port);
	//str.Copy(ss, len, str.Length());
	str.Append(':').Append(Port);

	return str;
}

const IPEndPoint& IPEndPoint::Any()
{
	static const IPEndPoint _Any(IPAddress::Any(), 0);
	return _Any;
}

bool operator==(const IPEndPoint& addr1, const IPEndPoint& addr2)
{
	return addr1.Port == addr2.Port && addr1.Address == addr2.Address;
}
bool operator!=(const IPEndPoint& addr1, const IPEndPoint& addr2)
{
	return addr1.Port != addr2.Port || addr1.Address != addr2.Address;
}

/******************************** MacAddress ********************************/
/* MAC地址 */

#define MAC_MASK 0xFFFFFFFFFFFFull

//const MacAddress MacAddress::Empty(0x0ull);
//const MacAddress MacAddress::Full(MAC_MASK);

MacAddress::MacAddress(ulong v)
{
	Value = v & MAC_MASK;
}

MacAddress::MacAddress(const byte* macs)
{
	/*ByteArray bs(macs, 6);
	Value = bs.ToUInt64() & MAC_MASK;*/
	memcpy((byte*)&Value, macs, 6);
}

MacAddress::MacAddress(const ByteArray& arr)
{
	Value = arr.ToUInt64();
	Value &= MAC_MASK;
}

// 是否广播地址，全0或全1
bool MacAddress::IsBroadcast() const { return Value == 0 || Value == MAC_MASK; }

const MacAddress& MacAddress::Empty()
{
	static const MacAddress _Empty(0x0ull);
	return _Empty;
}

const MacAddress& MacAddress::Full()
{
	static const MacAddress _Full(MAC_MASK);
	return _Full;
}

MacAddress& MacAddress::operator=(ulong v)
{
	Value = v & MAC_MASK;

	return *this;
}

MacAddress& MacAddress::operator=(const byte* buf)
{
	/*ByteArray bs(buf, 6);
	Value = bs.ToUInt64() & MAC_MASK;*/
	memcpy((byte*)&Value, buf, 6);

	return *this;
}

// 重载索引运算符[]，让它可以像数组一样使用下标索引。
byte& MacAddress::operator[](int i)
{
	assert_param(i >= 0 && i < 6);

	return ((byte*)&Value)[i];
}

// 字节数组
ByteArray MacAddress::ToArray() const
{
	//return ByteArray((byte*)&Value, 6);

	// 要复制数据，而不是直接使用指针，那样会导致外部修改内部数据
	return ByteArray((byte*)&Value, 6, true);
}

/*bool MacAddress::operator==(MacAddress& addr1, MacAddress& addr2)
{
	return addr1.v4 == addr2.v4 && addr1.v2 == addr2.v2;
}

bool MacAddress::operator!=(MacAddress& addr1, MacAddress& addr2)
{
	return addr1.v4 != addr2.v4 || addr1.v2 != addr2.v2;
}*/

String& MacAddress::ToStr(String& str) const
{
	byte* macs = (byte*)&Value;

	for(int i=0; i<6; i++)
	{
		if(i > 0) str.Append('-');
		str.Append(macs[i]);
	}

	return str;
}

/*void MacAddress::Show()
{
	byte* macs = (byte*)&Value;
	debug_printf("%02X-%02X-%02X-%02X-%02X-%02X", macs[0], macs[1], macs[2], macs[3], macs[4], macs[5]);
}*/
