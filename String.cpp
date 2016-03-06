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

	/*
	其实这里可以不用拷贝，内部直接使用这个指针，等第一次修改的时候再拷贝，不过那样过于复杂了
	*/
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
	/*char buf[2];
	buf[0] = c;
	buf[1] = 0;
	*this = buf;*/
	_Arr[0] = c;
	_Arr[1] = 0;
}

String::String(byte value, byte radix)
{
	init();

	utoa(value, _Arr, radix);
}

String::String(int value, byte radix)
{
	init();

	itoa(value, _Arr, radix);
}

String::String(uint value, byte radix)
{
	init();

	utoa(value, _Arr, radix);
}

String::String(Int64 value, byte radix)
{
	init();

	ltoa(value, _Arr, radix);
}

String::String(ulong value, byte radix)
{
	init();

	ultoa(value, _Arr, radix);
}

String::String(float value, byte decimalPlaces)
{
	init();

	dtostrf(value, (decimalPlaces + 2), decimalPlaces, _Arr);
}

String::String(double value, byte decimalPlaces)
{
	init();

	dtostrf(value, (decimalPlaces + 2), decimalPlaces, _Arr);
}

// 外部传入缓冲区供内部使用，注意长度减去零结束符
String::String(char* str, int length)
{
	init();

	_Arr		= str;
	_Capacity	= length - 1;
	_Arr[0]		= 0;
}

String::~String()
{
	if(_needFree && _Arr != Arr) delete _Arr;
}

inline void String::init()
{
	_Arr		= Arr;
	_Capacity	= sizeof(Arr) - 1;
	_Length		= 0;
	_needFree	= false;
	//_canWrite	= true;
}

void String::release()
{
	if(_needFree && _Arr != Arr) delete _Arr;

	init();
}

bool String::CheckCapacity(uint size)
{
	if (_Arr && _Capacity >= size) return true;

	// 外部需要放下size个字符，那么需要size+1个字节空间
	int sz	= _Capacity;
	while(sz <= size) sz <<= 1;

	auto p	= new char[sz];
	if(!p) return false;

	if(_Length)
		strcpy(p, _Arr);
	else
		p[0]	= 0;

	if(_needFree && _Arr != Arr) delete _Arr;

	_Arr		= p;
	_Capacity	= sz;
	_needFree	= true;

	return true;
}

String& String::copy(const char* cstr, uint length)
{
	if (!CheckCapacity(length))
		release();
	else
	{
		_Length = length;
		strcpy(_Arr, cstr);
	}

	return *this;
}

void String::move(String& rhs)
{
	if (_Arr) {
		if (_Capacity >= rhs._Length) {
			strcpy(_Arr, rhs._Arr);
			_Length = rhs._Length;
			rhs._Length = 0;
			return;
		} else {
			delete _Arr;
		}
	}
	_Arr		= rhs._Arr;
	_Capacity	= rhs._Capacity;
	_Length		= rhs._Length;
	rhs._Arr	= NULL;
	rhs._Capacity	= 0;
	rhs._Length	= 0;
}

void String::SetBuffer(const void* str, int length)
{
	_Arr		= (char*)str;
	_Capacity	= length;
	_Length		= 0;
}

String& String::operator = (const String& rhs)
{
	if (this == &rhs) return *this;

	if (rhs._Arr) copy(rhs._Arr, rhs._Length);
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
	return Concat(s._Arr, s._Length);
}

bool String::Concat(const char* cstr, uint length)
{
	uint newlen = _Length + length;
	if (!cstr) return false;
	if (length == 0) return true;
	if (!CheckCapacity(newlen)) return false;
	strcpy(_Arr + _Length, cstr);
	_Length = newlen;

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
	if (!a.Concat(str._Arr, str._Length)) a.release();
	return a;
}

StringHelper& operator + (const StringHelper& lhs, const String& rhs)
{
	auto& a = const_cast<StringHelper&>(lhs);
	if (!a.Concat(rhs._Arr, rhs._Length)) a.release();
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
	if (!_Arr || !s._Arr) {
		if (s._Arr && s._Length > 0) return 0 - *(byte*)s._Arr;
		if (_Arr && _Length > 0) return *(byte*)_Arr;
		return 0;
	}
	return strcmp(_Arr, s._Arr);
}

bool String::Equals(const String& s2) const
{
	return _Length == s2._Length && CompareTo(s2) == 0;
}

bool String::Equals(const char* cstr) const
{
	if (_Length == 0) return cstr == NULL || *cstr == 0;
	if (cstr == NULL) return _Arr[0] == 0;

	return strcmp(_Arr, cstr) == 0;
}

bool String::EqualsIgnoreCase(const String &s2 ) const
{
	if (this == &s2) return true;
	if (_Length != s2._Length) return false;
	if (_Length == 0) return true;
	const char *p1 = _Arr;
	const char *p2 = s2._Arr;
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
	if (loc < _Length) _Arr[loc] = c;
}

char& String::operator[](int index)
{
	static char dummy_writable_char;
	if (index >= _Length || !_Arr) {
		dummy_writable_char = 0;
		return dummy_writable_char;
	}
	return _Arr[index];
}

char String::operator[](int index) const
{
	if (index >= _Length || !_Arr) return 0;
	return _Arr[index];
}

void String::GetBytes(byte* buf, int bufsize, int index) const
{
	if (!bufsize || !buf) return;
	if (index >= _Length) {
		buf[0] = 0;
		return;
	}
	int n = bufsize - 1;
	if (n > _Length - index) n = _Length - index;
	strncpy((char*)buf, _Arr + index, n);
	buf[n] = 0;
}

ByteArray String::GetBytes() const
{
	ByteArray bs;
	bs.SetLength(_Length);

	GetBytes(bs.GetBuffer(), bs.Length());

	return bs;
}

ByteArray String::ToHex() const
{
	ByteArray bs;
	bs.SetLength(_Length / 2);

	char cs[3];
	cs[2]	= 0;
	byte* b	= bs.GetBuffer();
	char* p	= _Arr;
	for(int i=0; i<_Length; i+=2)
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
	//str.Copy(*this, str._Length);
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
	release();
}

// 调试输出字符串
void String::Show(bool newLine) const
{
	if(_Length) debug_printf("%s", _Arr);
	if(newLine) debug_printf("\r\n");
}

// 格式化字符串，输出到现有字符串后面。方便我们连续格式化多个字符串
String& String::Format(const char* format, ...)
{
	va_list ap;

	va_start(ap, format);

	// 无法准确估计长度，大概乘以2处理
	CheckCapacity(_Length + (strlen(format) << 1));

	//char* p = _Arr;
	int len2 = vsnprintf(_Arr + _Length, _Capacity - _Length, format, ap);
	_Length += len2;

	va_end(ap);

	return *this;
}

int String::IndexOf(const char ch, int startIndex) const
{
	if(startIndex >= _Length) return -1;

	auto p	= strchr(_Arr + startIndex, ch);
	if(!p) return -1;

	return p - _Arr;
}

int String::IndexOf(const String& str, int startIndex) const
{
	if(str._Length == 0) return -1;
	if(startIndex + str._Length > _Length) return -1;

	auto p	= strstr(_Arr + startIndex, str._Arr);
	if(!p) return -1;

	return p - _Arr;
}

int String::IndexOf(const char* str, int startIndex) const
{
	if(!str) return -1;
	if(startIndex + strlen(str) > _Length) return -1;

	auto p	= strstr(_Arr + startIndex, str);
	if(!p) return -1;

	return p - _Arr;
}

int String::LastIndexOf(const char ch, int startIndex) const
{
	if(startIndex >= _Length) return -1;

	auto p	= strrchr(_Arr + startIndex, ch);
	if(!p) return -1;

	return p - _Arr;
}

char *strrstr(const char* s, const char* str)
{
    char *p;
    int _Length = strlen(s);
    for (p = (char*)s + _Length - 1; p >= s; p--) {
        if ((*p == *str) && (memcmp(p, str, strlen(str)) == 0))
            return p;
    }
    return NULL;
}

int String::LastIndexOf(const String& str, int startIndex) const
{
	if(str._Length == 0) return -1;
	if(startIndex + str._Length > _Length) return -1;

	auto p	= strrstr(_Arr + startIndex, str._Arr);
	if(!p) return -1;

	return p - _Arr;
}

int String::LastIndexOf(const char* str, int startIndex) const
{
	if(!str) return -1;
	if(startIndex + strlen(str) > _Length) return -1;

	auto p	= strrstr(_Arr + startIndex, str);
	if(!p) return -1;

	return p - _Arr;
}

bool String::StartsWith(const String& str, int startIndex) const
{
	if (startIndex + str._Length > _Length || !_Arr || !str._Arr) return false;
	return strncmp(&_Arr[startIndex], str._Arr, str._Length) == 0;
}

bool String::StartsWith(const char* str, int startIndex) const
{
	if(!str) return false;
	int slen	= strlen(str);
	if (startIndex + slen > _Length || !_Arr) return false;
	return strncmp(&_Arr[startIndex], str, slen) == 0;
}

bool String::EndsWith(const String& str) const
{
	if(str._Length == 0) return false;
	if(str._Length > _Length) return false;

	return strncmp(&_Arr[_Length - str._Length], str._Arr, str._Length) == 0;
}

bool String::EndsWith(const char* str) const
{
	if(!str) return false;
	int slen	= strlen(str);
	if(slen > _Length) return false;

	return strncmp(&_Arr[_Length - slen], str, slen) == 0;
}

int String::Split(const String& str, StringItem callback)
{
	if(str.Length() == 0) return 0;

	int n	= 0;
	int p	= 0;
	int e	= 0;
	while(p < _Length)
	{
		// 找到下一个位置。如果找不到，直接移到末尾
		e	= IndexOf(str, p);
		if(e < 0) e = _Length;

		n++;

		auto item	= Substring(p, e - p);
		callback(item);

		// 如果在末尾，说明没有找到
		if(e == _Length) break;

		p	= e + str.Length();
	}

	return n;
}

String String::Substring(int start, int length) const
{
	String str;
	//str.Copy(this, _Length, start);
	if(_Length && start < _Length) str.copy(_Arr + start, length);

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
	trim(_Arr, _Length, true, false);

	return *this;
}

String& String::TrimEnd()
{
	trim(_Arr, _Length, false, true);

	return *this;
}

String& String::Trim()
{
	trim(_Arr, _Length, true, true);

	return *this;
}

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
