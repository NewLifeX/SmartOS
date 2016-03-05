#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>

#include "String.h"

extern char* itoa(int value, char* string, int radix);
extern char* ltoa(Int64 value, char* string, int radix);
extern char* utoa(uint value, char* string, int radix);
extern char* ultoa(ulong value, char* string, int radix);
char* dtostrf(double val, char width, byte prec, char* sout);

/******************************** String ********************************/

String::String(const char* cstr)
{
	init();
	if (cstr) copy(cstr, strlen(cstr));
}

String::String(const String& value)
{
	init();
	*this = value;
}

String::String(String&& rval)
{
	init();
	move(rval);
}
String::String(StringHelper&& rval)
{
	init();
	move(rval);
}

String::String(char c)
{
	init();
	char buf[2];
	buf[0] = c;
	buf[1] = 0;
	*this = buf;
}

String::String(byte value, byte radix)
{
	init();
	char buf[1 + 8 * sizeof(byte)];
	utoa(value, buf, radix);
	*this = buf;
}

String::String(int value, byte radix)
{
	init();
	char buf[2 + 8 * sizeof(int)];
	itoa(value, buf, radix);
	*this = buf;
}

String::String(uint value, byte radix)
{
	init();
	char buf[1 + 8 * sizeof(uint)];
	utoa(value, buf, radix);
	*this = buf;
}

String::String(Int64 value, byte radix)
{
	init();
	char buf[2 + 8 * sizeof(Int64)];
	ltoa(value, buf, radix);
	*this = buf;
}

String::String(ulong value, byte radix)
{
	init();
	char buf[1 + 8 * sizeof(ulong)];
	ultoa(value, buf, radix);
	*this = buf;
}

String::String(float value, byte decimalPlaces)
{
	init();
	char buf[33];
	*this = dtostrf(value, (decimalPlaces + 2), decimalPlaces, buf);
}

String::String(double value, byte decimalPlaces)
{
	init();
	char buf[33];
	*this = dtostrf(value, (decimalPlaces + 2), decimalPlaces, buf);
}

String::String(char* str, int length)
{
	buffer		= str;
	capacity	= length;
	len			= 0;
	buffer[0]	= 0;
}

String::~String()
{
	delete buffer;
}

inline void String::init()
{
	buffer		= Arr;
	capacity	= sizeof(Arr);
	len			= 0;
}

void String::release()
{
	delete buffer;

	init();
}

bool String::CheckCapacity(uint size)
{
	if (buffer && capacity >= size) return true;

	auto p	= new char[size + 1];
	if(!p) return false;

	if(len)
		strncpy(p, buffer, len);
	else
		p[0]	= 0;

	delete buffer;
	buffer		= p;
	capacity	= size;

	return true;
}

String& String::copy(const char* cstr, uint length)
{
	if (!CheckCapacity(length))
		release();
	else
	{
		len = length;
		strcpy(buffer, cstr);
	}

	return *this;
}

void String::move(String& rhs)
{
	if (buffer) {
		if (capacity >= rhs.len) {
			strcpy(buffer, rhs.buffer);
			len = rhs.len;
			rhs.len = 0;
			return;
		} else {
			delete buffer;
		}
	}
	buffer		= rhs.buffer;
	capacity	= rhs.capacity;
	len	= rhs.len;
	rhs.buffer	= NULL;
	rhs.capacity	= 0;
	rhs.len	= 0;
}

void String::SetBuffer(const void* str, int length)
{
	buffer		= (char*)str;
	capacity	= length;
	len			= 0;
}

String& String::operator = (const String& rhs)
{
	if (this == &rhs) return *this;

	if (rhs.buffer) copy(rhs.buffer, rhs.len);
	else release();

	return *this;
}

String& String::operator = (String&& rval)
{
	if (this != &rval) move(rval);
	return *this;
}

String& String::operator = (StringHelper&& rval)
{
	if (this != &rval) move(rval);
	return *this;
}

String& String::operator = (const char* cstr)
{
	if (cstr) copy(cstr, strlen(cstr));
	else release();

	return *this;
}

bool String::Concat(const Object& obj)
{
	return Concat(obj.ToString());
}

bool String::Concat(const String& s)
{
	return Concat(s.buffer, s.len);
}

bool String::Concat(const char* cstr, uint length)
{
	uint newlen = len + length;
	if (!cstr) return false;
	if (length == 0) return true;
	if (!CheckCapacity(newlen)) return false;
	strcpy(buffer + len, cstr);
	len = newlen;

	return true;
}

bool String::Concat(const char* cstr)
{
	if (!cstr) return 0;
	return Concat(cstr, strlen(cstr));
}

bool String::Concat(char c)
{
	char buf[2];
	buf[0] = c;
	buf[1] = 0;
	return Concat(buf, 1);
}

bool String::Concat(byte num, byte radix)
{
	char buf[1 + 3 * sizeof(byte)];
	itoa(num, buf, radix);
	return Concat(buf, strlen(buf));
}

bool String::Concat(int num, byte radix)
{
	char buf[2 + 3 * sizeof(int)];
	itoa(num, buf, radix);
	return Concat(buf, strlen(buf));
}

/*bool String::Concat(uint num, byte radix, byte width)
{
	char buf[2 + 3 * sizeof(int)];
	itoa(num, buf, radix);
	int length	= strlen(buf);
	if(width)
	{
		int remain	= width - length;
		if(remain > 0)
		{
			// 前面补0
			char cs[16];
			memset(cs, '0', remain);
			cs[remain]	= 0;
			Concat(cs, remain);
		}
		// 如果超长，按照C#标准，让它撑大
	}
	return Concat(buf, length);
}*/

bool String::Concat(uint num, byte radix)
{
	char buf[1 + 3 * sizeof(uint)];
	utoa(num, buf, radix);
	return Concat(buf, strlen(buf));
}

bool String::Concat(Int64 num, byte radix)
{
	char buf[2 + 3 * sizeof(Int64)];
	ltoa(num, buf, radix);
	return Concat(buf, strlen(buf));
}

bool String::Concat(ulong num, byte radix)
{
	char buf[1 + 3 * sizeof(ulong)];
	ultoa(num, buf, radix);
	return Concat(buf, strlen(buf));
}

bool String::Concat(float num)
{
	char buf[20];
	char* string = dtostrf(num, 4, 2, buf);
	return Concat(string, strlen(string));
}

bool String::Concat(double num)
{
	char buf[20];
	char* string = dtostrf(num, 4, 2, buf);
	return Concat(string, strlen(string));
}

StringHelper& operator + (const StringHelper& lhs, const Object& rhs)
{
	auto& a = const_cast<StringHelper&>(lhs);
	auto str = rhs.ToString();
	if (!a.Concat(str.buffer, str.len)) a.release();
	return a;
}

StringHelper& operator + (const StringHelper& lhs, const String& rhs)
{
	auto& a = const_cast<StringHelper&>(lhs);
	if (!a.Concat(rhs.buffer, rhs.len)) a.release();
	return a;
}

StringHelper& operator + (const StringHelper& lhs, const char* cstr)
{
	auto& a = const_cast<StringHelper&>(lhs);
	if (!cstr || !a.Concat(cstr, strlen(cstr))) a.release();
	return a;
}

StringHelper& operator + (const StringHelper& lhs, char c)
{
	auto& a = const_cast<StringHelper&>(lhs);
	if (!a.Concat(c)) a.release();
	return a;
}

StringHelper& operator + (const StringHelper& lhs, byte num)
{
	auto& a = const_cast<StringHelper&>(lhs);
	if (!a.Concat(num)) a.release();
	return a;
}

StringHelper& operator + (const StringHelper& lhs, int num)
{
	auto& a = const_cast<StringHelper&>(lhs);
	if (!a.Concat(num)) a.release();
	return a;
}

StringHelper& operator + (const StringHelper& lhs, uint num)
{
	auto& a = const_cast<StringHelper&>(lhs);
	if (!a.Concat(num)) a.release();
	return a;
}

StringHelper& operator + (const StringHelper& lhs, Int64 num)
{
	auto& a = const_cast<StringHelper&>(lhs);
	if (!a.Concat(num)) a.release();
	return a;
}

StringHelper& operator + (const StringHelper& lhs, ulong num)
{
	auto& a = const_cast<StringHelper&>(lhs);
	if (!a.Concat(num)) a.release();
	return a;
}

StringHelper& operator + (const StringHelper& lhs, float num)
{
	auto& a = const_cast<StringHelper&>(lhs);
	if (!a.Concat(num)) a.release();
	return a;
}

StringHelper& operator + (const StringHelper& lhs, double num)
{
	auto& a = const_cast<StringHelper&>(lhs);
	if (!a.Concat(num)) a.release();
	return a;
}

int String::CompareTo(const String& s) const
{
	if (!buffer || !s.buffer) {
		if (s.buffer && s.len > 0) return 0 - *(byte*)s.buffer;
		if (buffer && len > 0) return *(byte*)buffer;
		return 0;
	}
	return strcmp(buffer, s.buffer);
}

bool String::Equals(const String& s2) const
{
	return len == s2.len && CompareTo(s2) == 0;
}

bool String::Equals(const char* cstr) const
{
	if (len == 0) return cstr == NULL || *cstr == 0;
	if (cstr == NULL) return buffer[0] == 0;

	return strcmp(buffer, cstr) == 0;
}

bool String::EqualsIgnoreCase(const String &s2 ) const
{
	if (this == &s2) return true;
	if (len != s2.len) return false;
	if (len == 0) return true;
	const char *p1 = buffer;
	const char *p2 = s2.buffer;
	while (*p1) {
		if (tolower(*p1++) != tolower(*p2++)) return false;
	}
	return true;
}

bool String::operator<(const String& rhs) const
{
	return CompareTo(rhs) < 0;
}

bool String::operator>(const String& rhs) const
{
	return CompareTo(rhs) > 0;
}

bool String::operator<=(const String& rhs) const
{
	return CompareTo(rhs) <= 0;
}

bool String::operator>=(const String& rhs) const
{
	return CompareTo(rhs) >= 0;
}

void String::SetAt(int loc, char c)
{
	if (loc < len) buffer[loc] = c;
}

char& String::operator[](int index)
{
	static char dummy_writable_char;
	if (index >= len || !buffer) {
		dummy_writable_char = 0;
		return dummy_writable_char;
	}
	return buffer[index];
}

char String::operator[]( int index ) const
{
	if (index >= len || !buffer) return 0;
	return buffer[index];
}

void String::GetBytes(byte* buf, int bufsize, int index) const
{
	if (!bufsize || !buf) return;
	if (index >= len) {
		buf[0] = 0;
		return;
	}
	int n = bufsize - 1;
	if (n > len - index) n = len - index;
	strncpy((char*)buf, buffer + index, n);
	buf[n] = 0;
}

ByteArray String::GetBytes() const
{
	ByteArray bs;
	bs.SetLength(len);

	GetBytes(bs.GetBuffer(), bs.Length());

	return bs;
}

ByteArray String::ToHex() const
{
	ByteArray bs;
	bs.SetLength(len / 2);

	char cs[3];
	cs[2]	= 0;
	byte* b	= bs.GetBuffer();
	char* p	= buffer;
	for(int i=0; i<len; i+=2)
	{
		cs[0]	= *p++;
		cs[1]	= *p++;

		*b++	= (byte)strtol(cs, nullptr, 16);
	}

	return bs;
}

// 输出对象的字符串表示方式
String& String::ToStr(String& str) const
{
	// 把当前字符串复制到目标字符串后面
	//str.Copy(*this, str.len);
	str	+= *this;

	return (String&)*this;
}

// 输出对象的字符串表示方式
String String::ToString() const
{
	return *this;
}

// 清空已存储数据。
void String::Clear()
{
	/*TArray::Clear();

	_Length = 0;*/
	release();
}

/*String& String::Append(char ch)
{
	Copy(&ch, 1, len);

	return *this;
}

String& String::Append(const char* str, int len)
{
	Copy(str, len, len);

	return *this;
}*/

/*char* _itoa(int value, char* str, int radix, int width = 0)
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
}*/

/*// 写入整数，第二参数指定宽带，不足时补零
String& String::Append(int value, int radix, int width)
{
	char ch[16];
	_itoa(value, ch, radix, width);

	return Append(ch);
}

String& String::Append(byte bt)
{
	int k = len;

	byte b = bt >> 4;
	SetAt(k++, b > 9 ? ('A' + b - 10) : ('0' + b));
	b = bt & 0x0F;
	SetAt(k++, b > 9 ? ('A' + b - 10) : ('0' + b));

	return *this;
}

String& String::Append(const ByteArray& bs)
{
	bs.ToHex(*this);

	return *this;
}*/

// 调试输出字符串
void String::Show(bool newLine) const
{
	//if(!len) return;

	// C格式字符串以0结尾
	//char* p = buffer;
	//if(p[len] != 0 && !IN_ROM_SECTION(p)) p[len] = 0;

	if(len) debug_printf("%s", buffer);
	if(newLine) debug_printf("\r\n");
}

// 格式化字符串，输出到现有字符串后面。方便我们连续格式化多个字符串
String& String::Format(const char* format, ...)
{
	va_list ap;

	//const char* fmt = format.buffer;
	va_start(ap, format);

	// 无法准确估计长度，大概乘以2处理
	CheckCapacity(len + (strlen(format) << 1));

	//char* p = buffer;
	int len2 = vsnprintf(buffer + len, capacity - len, format, ap);
	len += len2;

	va_end(ap);

	return *this;
}

/*String& String::Concat(const Object& obj)
{
	obj.ToStr(*this);

	return *this;
}

String& String::Concat(const char* str, int len)
{
	Copy(str, len, len);

	return *this;
}*/

int String::IndexOf(const char ch, int startIndex) const
{
	if(startIndex >= len) return -1;

	auto p	= strchr(buffer + startIndex, ch);
	if(!p) return -1;

	return p - buffer;
}

int String::IndexOf(const String& str, int startIndex) const
{
	if(str.len == 0) return -1;
	if(startIndex + str.len > len) return -1;

	auto p	= strstr(buffer + startIndex, str.buffer);
	if(!p) return -1;

	return p - buffer;
}

int String::IndexOf(const char* str, int startIndex) const
{
	if(!str) return -1;
	if(startIndex + strlen(str) > len) return -1;

	auto p	= strstr(buffer + startIndex, str);
	if(!p) return -1;

	return p - buffer;
}

int String::LastIndexOf(const char ch, int startIndex) const
{
	if(startIndex >= len) return -1;

	auto p	= strrchr(buffer + startIndex, ch);
	if(!p) return -1;

	return p - buffer;
}

char *strrstr(const char* s, const char* str)
{
    char *p;
    int len = strlen(s);
    for (p = (char*)s + len - 1; p >= s; p--) {
        if ((*p == *str) && (memcmp(p, str, strlen(str)) == 0))
            return p;
    }
    return NULL;
}

int String::LastIndexOf(const String& str, int startIndex) const
{
	if(str.len == 0) return -1;
	if(startIndex + str.len > len) return -1;

	auto p	= strrstr(buffer + startIndex, str.buffer);
	if(!p) return -1;

	return p - buffer;
}

int String::LastIndexOf(const char* str, int startIndex) const
{
	if(!str) return -1;
	if(startIndex + strlen(str) > len) return -1;

	auto p	= strrstr(buffer + startIndex, str);
	if(!p) return -1;

	return p - buffer;
}

bool String::StartsWith(const String& str, int startIndex) const
{
	//return Sub(startIndex, str.len) == str;
	if (startIndex + str.len > len || !buffer || !str.buffer) return false;
	return strncmp(&buffer[startIndex], str.buffer, str.len) == 0;
}

bool String::StartsWith(const char* str, int startIndex) const
{
	//return Sub(startIndex, strlen(str)) == str;
	if(!str) return false;
	int slen	= strlen(str);
	if (startIndex + slen > len || !buffer) return false;
	return strncmp(&buffer[startIndex], str, slen) == 0;
}

bool String::EndsWith(const String& str) const
{
	//return Sub(len - str.len, str.len) == str;
	if(str.len == 0) return false;
	if(str.len > len) return false;

	return strncmp(&buffer[len - str.len], str.buffer, str.len) == 0;
}

bool String::EndsWith(const char* str) const
{
	//return Sub(len - strlen(str), strlen(str)) == str;
	if(!str) return false;
	int slen	= strlen(str);
	if(slen > len) return false;

	return strncmp(&buffer[len - slen], str, slen) == 0;
}

String String::Substring(int start, int length) const
{
	String str;
	//str.Copy(this, len, start);
	if(len && start < len) str.copy(buffer + start, length);

	return str;
}

void trim(char* buffer, int& len, bool trimBegin, bool trimEnd)
{
	if (!buffer || len == 0) return;
	char *begin = buffer;
	if(trimBegin) while (isspace(*begin)) begin++;
	char *end = buffer + len - 1;
	if(trimEnd) while (isspace(*end) && end >= begin) end--;
	len = end + 1 - begin;
	if (begin > buffer) memcpy(buffer, begin, len);
	buffer[len] = 0;
}

String& String::TrimStart()
{
	/*String str;

	auto ptr	= buffer;
	// 找到第一个不是空白字符的位置
	int i = 0;
	for(;i < len; i++)
	{
		auto ch	= ptr[i];
		if(ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') break;
	}
	str.copy(ptr + i, len - i);

	return str;*/

	trim(buffer, len, true, false);

	return *this;
}

String& String::TrimEnd()
{
	/*String str;

	auto ptr	= buffer;
	// 找到最后一个不是空白字符的位置
	int i = len - 1;
	for(;i >= 0; i--)
	{
		auto ch	= ptr[i];
		if(ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') break;
	}
	str.copy(ptr, i + 1);

	return str;*/

	/*if (!buffer || len == 0) return;
	char *begin = buffer;
	//while (isspace(*begin)) begin++;
	char *end = buffer + len - 1;
	while (isspace(*end) && end >= begin) end--;
	len = end + 1 - begin;
	if (begin > buffer) memcpy(buffer, begin, len);
	buffer[len] = 0;*/

	trim(buffer, len, false, true);

	return *this;
}

String& String::Trim()
{
	//return TrimStart().TrimEnd();;

	/*if (!buffer || len == 0) return;
	char *begin = buffer;
	while (isspace(*begin)) begin++;
	char *end = buffer + len - 1;
	while (isspace(*end) && end >= begin) end--;
	len = end + 1 - begin;
	if (begin > buffer) memcpy(buffer, begin, len);
	buffer[len] = 0;*/

	trim(buffer, len, true, true);

	return *this;
}

/*String& String::operator+=(const Object& obj)
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

String operator+(const char* str1, const char* str2)
{
	String str(str1);
	return str + str2;
}*/

/*String operator+(const char* str, const Object& obj)
{
	// 要把字符串拷贝过来
	String s;
	s = str;
	s += obj;
	return s;
}*/

/*String operator+(const Object& obj, const char* str)
{
	// 要把字符串拷贝过来
	String s;
	obj.ToStr(s);
	s += str;
	return s;
}*/

extern char* itoa(int value, char *string, int radix)
{
	return ltoa(value, string, radix) ;
}

extern char* ltoa(Int64 value, char* string, int radix )
{
	char tmp[33];
	char *tp = tmp;
	Int64 i;
	ulong v;
	int sign;
	char *sp;

	if ( string == NULL ) return 0 ;

	if (radix > 36 || radix <= 1) return 0 ;

	sign = (radix == 10 && value < 0);
	if (sign)
		v = -value;
	else
		v = (ulong)value;

	while (v || tp == tmp)
	{
		i = v % radix;
		v = v / radix;
		if (i < 10)
			*tp++ = i+'0';
		else
			*tp++ = i + 'a' - 10;
	}

	sp = string;

	if (sign) *sp++ = '-';
	while (tp > tmp)
		*sp++ = *--tp;
	*sp = 0;

	return string;
}

extern char* utoa(uint value, char* string, int radix)
{
	return ultoa(value, string, radix ) ;
}

extern char* ultoa(ulong value, char* string, int radix)
{
	char tmp[33];
	char *tp = tmp;
	Int64 i;
	ulong v = value;
	char *sp;

	if ( string == NULL ) return 0;

	if (radix > 36 || radix <= 1) return 0;

	while (v || tp == tmp)
	{
		i = v % radix;
		v = v / radix;
		if (i < 10)
			*tp++ = i+'0';
		else
			*tp++ = i + 'a' - 10;
	}

	sp = string;

	while (tp > tmp)
		*sp++ = *--tp;
	*sp = 0;

	return string;
}

char *dtostrf (double val, char width, byte prec, char* sout)
{
	char fmt[20];
	sprintf(fmt, "%%%d.%df", width, prec);
	sprintf(sout, fmt, val);
	return sout;
}
