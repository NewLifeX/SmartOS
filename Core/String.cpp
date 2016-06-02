#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>

#include "_Core.h"

#include "ByteArray.h"

#include "SString.h"

char* utohex(uint value, byte size, char* string, bool upper);
extern char* itoa(int value, char* string, int radix);
extern char* ltoa(Int64 value, char* string, int radix);
extern char* utoa(uint value, char* string, int radix);
extern char* ultoa(UInt64 value, char* string, int radix);
char* dtostrf(double val, char width, byte prec, char* sout);

/******************************** String ********************************/

String::String(cstring cstr) : Array(Arr, ArrayLength(Arr))
{
	init();

	_Length	= strlen(cstr);
	if(_Length)
	{
		_Arr	= (char*)cstr;
		// 此时保证外部一定是0结尾
		_Capacity	= _Length + 1;
		_canWrite	= false;
	}
}

String::String(const String& value) : Array(Arr, ArrayLength(Arr))
{
	init();
	*this = value;
}

String::String(String&& rval) : Array(Arr, ArrayLength(Arr))
{
	init();
	move(rval);
}

String::String(char c) : Array(Arr, ArrayLength(Arr))
{
	init();

	_Arr[0] = c;
	_Arr[1] = 0;
	_Length	= 1;
}

String::String(byte value, int radix) : Array(Arr, ArrayLength(Arr))
{
	init();

	Concat(value, radix);
}

String::String(short value, int radix) : Array(Arr, ArrayLength(Arr))
{
	init();

	Concat(value, radix);
}

String::String(ushort value, int radix) : Array(Arr, ArrayLength(Arr))
{
	init();

	Concat(value, radix);
}

String::String(int value, int radix) : Array(Arr, ArrayLength(Arr))
{
	init();

	Concat(value, radix);
}

String::String(uint value, int radix) : Array(Arr, ArrayLength(Arr))
{
	init();

	Concat(value, radix);
}

String::String(Int64 value, int radix) : Array(Arr, ArrayLength(Arr))
{
	init();

	Concat(value, radix);
}

String::String(UInt64 value, int radix) : Array(Arr, ArrayLength(Arr))
{
	init();

	Concat(value, radix);
}

String::String(float value, byte decimalPlaces) : Array(Arr, ArrayLength(Arr))
{
	init();

	Concat(value, decimalPlaces);
	//dtostrf(value, (decimalPlaces + 2), decimalPlaces, _Arr);
}

String::String(double value, byte decimalPlaces) : Array(Arr, ArrayLength(Arr))
{
	init();

	Concat(value, decimalPlaces);
	//dtostrf(value, (decimalPlaces + 2), decimalPlaces, _Arr);
}

// 外部传入缓冲区供内部使用，注意长度减去零结束符
String::String(char* str, int length) : Array(str, length)
{
	//init();

	_Arr		= str;
	_Capacity	= length - 1;
	_Length		= 0;
	_Arr[0]		= '\0';
}

// 包装静态字符串，直接使用，修改时扩容
String::String(cstring str, int length) : Array((char*)str, length)
{
	// 此时不能保证外部一定是0结尾
	_Capacity	= length + 1;
	_canWrite	= false;
}

inline void String::init()
{
	_Arr		= Arr;
	_Capacity	= sizeof(Arr) - 1;
	_Length		= 0;
	_Arr[0]		= '\0';
}

void String::release()
{
	//if(_needFree && _Arr != Arr) delete _Arr;
	Array::Release();

	init();
}

bool String::CheckCapacity(uint size)
{
	int old	= _Capacity;
	CheckCapacity(size + 1, _Length);
	if(old == _Capacity) return true;

	// 强制最后一个字符为0
	_Arr[_Length]	= '\0';

	_Capacity--;

	return true;
}

void* String::Alloc(int len)
{
	if(len <= sizeof(Arr))
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

String& String::copy(cstring cstr, uint length)
{
	if(!cstr || !length) return *this;

	if (!CheckCapacity(length))
		release();
	else
	{
		_Length = length;
		//strcpy(_Arr, cstr);
		//!!! 特别注意要拷贝的长度
		if(length) Buffer(_Arr, _Capacity).Copy(0, cstr, length);
		_Arr[length]	= '\0';
	}

	return *this;
}

void String::move(String& rhs)
{
	/*
	move逻辑：
	1，如果右值是内部指针，则必须拷贝数据，因为右值销毁的时候，内部数据跟着释放
	2，如果右值是外部指针，并且需要释放，则直接拿指针过来使用，由当前对象负责释放
	3，如果右值是外部指针，而不需要释放，则拷贝数据，因为那指针可能是借用外部的栈内存
	*/

	if(rhs._Arr != rhs.Arr && rhs._needFree)
	{
		Array::move(rhs);

		return;
	}

	SetLength(rhs.Length());
	copy(rhs._Arr, rhs._Length);
}

// 修改时拷贝
bool String::CopyOrWrite()
{
	// 如果不可写
	if(!_canWrite) return CheckCapacity(_Length);

	return false;
}

/*void String::SetBuffer(const void* str, int length)
{
	release();

	_Arr		= (char*)str;
	_Capacity	= length;
	_Length		= 0;

	_needFree	= false;
	_canWrite	= false;
}*/

bool String::SetLength(int len, bool bak)
{
	//if(!Array::SetLength(length, bak)) return false;
	// 字符串的最大长度为容量减一，因为需要保留一个零结束字符
	if(len < _Capacity)
	{
		_Length = len;
	}
	else
	{
		if(!CheckCapacity(len + 1, bak ? _Length : 0)) return false;
		// 扩大长度
		if(len > _Length) _Length = len;
	}

	_Arr[_Length]	= '\0';

	return true;
}

// 拷贝数据，默认-1长度表示当前长度
int String::Copy(int destIndex, const void* src, int len)
{
	int rs	= Buffer::Copy(destIndex, src, len);
	if(!rs) return 0;

	_Arr[_Length]	= '\0';

	return rs;
}

/*// 拷贝数据，默认-1长度表示两者最小长度
int String::Copy(int destIndex, const Buffer& src, int srcIndex, int len)
{
	int rs	= Buffer::Copy(destIndex, src, srcIndex, len);
	if(!rs) return 0;

	_Arr[_Length]	= '\0';

	return rs;
}*/

// 把数据复制到目标缓冲区，默认-1长度表示当前长度
int String::CopyTo(int srcIndex, void* dest, int len) const
{
	int rs	= Buffer::CopyTo(srcIndex, dest, len);
	if(!rs) return 0;

	((char*)dest)[rs]	= '\0';

	return rs;
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

String& String::operator = (cstring cstr)
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

bool String::Concat(cstring cstr, uint length)
{
	if (!cstr) return false;
	if (length == 0) return true;

	uint newlen = _Length + length;
	if (!CheckCapacity(newlen)) return false;

	//strcpy(_Arr + _Length, cstr);
	Buffer(_Arr, _Capacity).Copy(_Length, cstr, length);
	_Length = newlen;
	_Arr[_Length]	= '\0';

	return true;
}

bool String::Concat(cstring cstr)
{
	if (!cstr) return 0;
	return Concat(cstr, strlen(cstr));
}

bool String::Concat(char c)
{
	if (!CheckCapacity(_Length + 1)) return false;

	_Arr[_Length++]	= c;

	return true;
}

bool String::Concat(byte num, int radix)
{
	// 十六进制固定长度
	if(radix == 16 || radix == -16)
	{
		if (!CheckCapacity(_Length + (sizeof(num) << 1))) return false;

		utohex(num, sizeof(num), _Arr + _Length, radix < 0);
		_Length	+= (sizeof(num) << 1);

		return true;
	}

	char buf[1 + 3 * sizeof(byte)];
	itoa(num, buf, radix);

	return Concat(buf, strlen(buf));
}

bool String::Concat(short num, int radix)
{
	// 十六进制固定长度
	if(radix == 16 || radix == -16) return Concat((ushort)num, radix);

	char buf[2 + 3 * sizeof(int)];
	itoa(num, buf, radix);
	return Concat(buf, strlen(buf));
}

bool String::Concat(ushort num, int radix)
{
	// 十六进制固定长度
	if(radix == 16 || radix == -16)
	{
		if (!CheckCapacity(_Length + (sizeof(num) << 1))) return false;

		utohex(num, sizeof(num), _Arr + _Length, radix < 0);
		_Length	+= (sizeof(num) << 1);

		return true;
	}

	char buf[2 + 3 * sizeof(int)];
	utoa(num, buf, radix);
	return Concat(buf, strlen(buf));
}

bool String::Concat(int num, int radix)
{
	// 十六进制固定长度
	if(radix == 16 || radix == -16) return Concat((uint)num, radix);

	char buf[2 + 3 * sizeof(int)];
	itoa(num, buf, radix);
	return Concat(buf, strlen(buf));
}

bool String::Concat(uint num, int radix)
{
	// 十六进制固定长度
	if(radix == 16 || radix == -16)
	{
		if (!CheckCapacity(_Length + (sizeof(num) << 1))) return false;

		utohex(num, sizeof(num), _Arr + _Length, radix < 0);
		_Length	+= (sizeof(num) << 1);

		return true;
	}

	char buf[1 + 3 * sizeof(uint)];
	utoa(num, buf, radix);
	return Concat(buf, strlen(buf));
}

bool String::Concat(Int64 num, int radix)
{
	// 十六进制固定长度
	if(radix == 16 || radix == -16) return Concat((UInt64)num, radix);

	char buf[2 + 3 * sizeof(Int64)];
	ltoa(num, buf, radix);
	return Concat(buf, strlen(buf));
}

bool String::Concat(UInt64 num, int radix)
{
	// 十六进制固定长度
	if(radix == 16 || radix == -16)
	{
		if (!CheckCapacity(_Length + (sizeof(num) << 1))) return false;

		utohex((int)(num >> 32), sizeof(num) >> 1, _Arr + _Length, radix < 0);
		_Length	+= sizeof(num);
		utohex((int)(num & 0xFFFFFFFF), sizeof(num) >> 1, _Arr + _Length, radix < 0);
		_Length	+= sizeof(num);

		return true;
	}

	char buf[1 + 3 * sizeof(UInt64)];
	ultoa(num, buf, radix);
	return Concat(buf, strlen(buf));
}

bool String::Concat(float num, byte decimalPlaces)
{
	char buf[20];
	auto string = dtostrf(num, (decimalPlaces + 2), decimalPlaces, buf);
	return Concat(string, strlen(string));
}

bool String::Concat(double num, byte decimalPlaces)
{
	char buf[20];
	auto string = dtostrf(num, (decimalPlaces + 2), decimalPlaces, buf);
	return Concat(string, strlen(string));
}

String& operator + (String& lhs, const Object& rhs)
{
	auto& a = const_cast<String&>(lhs);
	auto str = rhs.ToString();
	if (!a.Concat(str._Arr, str._Length)) a.release();
	return a;
}

String& operator + (String& lhs, const String& rhs)
{
	auto& a = const_cast<String&>(lhs);
	if (!a.Concat(rhs._Arr, rhs._Length)) a.release();
	return a;
}

String& operator + (String& lhs, cstring cstr)
{
	auto& a = const_cast<String&>(lhs);
	if (!cstr || !a.Concat(cstr, strlen(cstr))) a.release();
	return a;
}

String& operator + (String& lhs, char c)
{
	auto& a = const_cast<String&>(lhs);
	if (!a.Concat(c)) a.release();
	return a;
}

String& operator + (String& lhs, byte num)
{
	auto& a = const_cast<String&>(lhs);
	if (!a.Concat(num)) a.release();
	return a;
}

String& operator + (String& lhs, int num)
{
	auto& a = const_cast<String&>(lhs);
	if (!a.Concat(num)) a.release();
	return a;
}

String& operator + (String& lhs, uint num)
{
	auto& a = const_cast<String&>(lhs);
	if (!a.Concat(num)) a.release();
	return a;
}

String& operator + (String& lhs, Int64 num)
{
	auto& a = const_cast<String&>(lhs);
	if (!a.Concat(num)) a.release();
	return a;
}

String& operator + (String& lhs, UInt64 num)
{
	auto& a = const_cast<String&>(lhs);
	if (!a.Concat(num)) a.release();
	return a;
}

String& operator + (String& lhs, float num)
{
	auto& a = const_cast<String&>(lhs);
	if (!a.Concat(num)) a.release();
	return a;
}

String& operator + (String& lhs, double num)
{
	auto& a = const_cast<String&>(lhs);
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

bool String::Equals(cstring cstr) const
{
	if (_Length == 0) return cstr == nullptr || *cstr == 0;
	if (cstr == nullptr) return _Arr[0] == 0;

	return strcmp(_Arr, cstr) == 0;
}

bool String::EqualsIgnoreCase(const String &s2 ) const
{
	if (this == &s2) return true;
	if (_Length != s2._Length) return false;
	if (_Length == 0) return true;
	auto p1	= _Arr;
	auto p2	= s2._Arr;
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

	// 修改时拷贝
	CopyOrWrite();

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
	int n = bufsize;
	if (n > _Length - index) n = _Length - index;
	//strncpy((char*)buf, _Arr + index, n);
	Buffer(buf, bufsize).Copy(0, _Arr + index, n);
	//buf[n] = '\0';
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
	auto p	= _Arr;
	int n	= 0;
	for(int i=0; i<_Length; i+=2)
	{
		cs[0]	= *p++;
		cs[1]	= *p++;

		*b++	= (byte)strtol(cs, nullptr, 16);

		// 过滤横杠和空格
		if(*p == '-' || isspace(*p))
		{
			p++;
			i++;
		}

		n++;
	}
	bs.SetLength(n);

	return bs;
}

int String::ToInt() const
{
	if(_Length == 0) return 0;

	return atoi(_Arr);
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

/*// 清空已存储数据。
void String::Clear()
{
	release();
}*/

// 调试输出字符串
void String::Show(bool newLine) const
{
	if(_Length) debug_printf("%s", _Arr);
	if(newLine) debug_printf("\r\n");
}

// 格式化字符串，输出到现有字符串后面。方便我们连续格式化多个字符串
String& String::Format(cstring format, ...)
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
	if(startIndex < 0) return -1;
	if(startIndex >= _Length) return -1;

	auto p	= strchr(_Arr + startIndex, ch);
	if(!p) return -1;

	return p - _Arr;
}

int String::IndexOf(const String& str, int startIndex) const
{
	if(str._Length == 0) return -1;
	if(startIndex < 0) return -1;
	if(startIndex + str._Length > _Length) return -1;

	auto p	= strstr(_Arr + startIndex, str._Arr);
	if(!p) return -1;

	return p - _Arr;
}

int String::IndexOf(cstring str, int startIndex) const
{
	if(!str) return -1;
	if(startIndex < 0) return -1;
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

char *strrstr(cstring s, cstring str)
{
    char *p;
    int _Length = strlen(s);
    for (p = (char*)s + _Length - 1; p >= s; p--) {
        if ((*p == *str) && (memcmp(p, str, strlen(str)) == 0))
            return p;
    }
    return nullptr;
}

int String::LastIndexOf(const String& str, int startIndex) const
{
	if(str._Length == 0) return -1;
	if(startIndex + str._Length > _Length) return -1;

	auto p	= strrstr(_Arr + startIndex, str._Arr);
	if(!p) return -1;

	return p - _Arr;
}

int String::LastIndexOf(cstring str, int startIndex) const
{
	if(!str) return -1;
	if(startIndex + strlen(str) > _Length) return -1;

	auto p	= strrstr(_Arr + startIndex, str);
	if(!p) return -1;

	return p - _Arr;
}

bool String::Contains(const String& str) const { return IndexOf(str) >= 0; }

bool String::Contains(cstring str) const { return IndexOf(str) >= 0; }

bool String::StartsWith(const String& str, int startIndex) const
{
	if (startIndex + str._Length > _Length || !_Arr || !str._Arr) return false;
	return strncmp(&_Arr[startIndex], str._Arr, str._Length) == 0;
}

bool String::StartsWith(cstring str, int startIndex) const
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

bool String::EndsWith(cstring str) const
{
	if(!str) return false;
	int slen	= strlen(str);
	if(slen > _Length) return false;

	return strncmp(&_Arr[_Length - slen], str, slen) == 0;
}

StringSplit String::Split(const String& sep) const
{
	return StringSplit(*this, sep);
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
	if (begin > buffer && len) Buffer(buffer, len)	= begin;
	buffer[len] = 0;
}

String String::TrimStart() const
{
	String str(*this);
	trim(str._Arr, str._Length, true, false);

	return str;
}

String String::TrimEnd() const
{
	String str(*this);
	trim(str._Arr, str._Length, false, true);

	return str;
}

String String::Trim() const
{
	String str(*this);
	trim(str._Arr, str._Length, true, true);

	return str;
}

String String::Replace(char find, char replace) const
{
	String str(*this);

	auto p	= (char*)str.GetBuffer();
	for(int i=0; i<Length(); i++, p++)
	{
		if(*p == find) *p	= replace;
	}

	return str;
}

String String::ToLower() const
{
	String str(*this);
	auto p	= str._Arr;
	for(int i=0; i<str._Length; i++)
		p[i]	= tolower(p[i]);

	return str;
}

String String::ToUpper() const
{
	String str(*this);
	auto p	= str._Arr;
	for(int i=0; i<str._Length; i++)
		p[i]	= toupper(p[i]);

	return str;
}

/******************************** 辅助 ********************************/

extern char* itoa(int value, char *string, int radix)
{
	return ltoa(value, string, radix) ;
}

extern char* ltoa(Int64 value, char* string, int radix)
{
	char tmp[33];
	char *tp = tmp;
	Int64 i;
	UInt64 v;
	int sign;
	char *sp;

	if ( string == nullptr ) return 0 ;

	if (radix > 36 || radix <= 1) return 0 ;

	sign = (radix == 10 && value < 0);
	if (sign)
		v = -value;
	else
		v = (UInt64)value;

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

char* utohex(uint value, byte size, char* string, bool upper)
{
	if (string == nullptr ) return 0;

	// 字节数乘以2是字符个数
	size	<<= 1;
	// 指针提前指向最后一个字符，数字从小往大处理，字符需要倒过来赋值
	auto tp	= string + size;;
	*tp--	= '\0';
	char ch	= upper ? 'A' : 'a';
	for(int i=0; i<size; i++)
	{
		byte bt = value & 0x0F;
		value	>>= 4;
		if (bt < 10)
			*tp-- = bt + '0';
		else
			*tp-- = bt + ch - 10;
	}

	return string;
}

extern char* utoa(uint value, char* string, int radix)
{
	return ultoa(value, string, radix ) ;
}

extern char* ultoa(UInt64 value, char* string, int radix)
{
	if (string == nullptr ) return 0;

	if (radix > 36 || radix <= 1) return 0;

	char tmp[33];
	auto tp	= tmp;
	auto v	= value;
	char ch	= radix < 0 ? 'A' : 'a';
	while (v || tp == tmp)
	{
		auto i = v % radix;
		v = v / radix;
		if (i < 10)
			*tp++ = i + '0';
		else
			*tp++ = i + ch - 10;
	}

	auto sp = string;
	while (tp > tmp)
		*sp++ = *--tp;
	*sp = '\0';

	return string;
}

char *dtostrf (double val, char width, byte prec, char* sout)
{
	char fmt[20];
	sprintf(fmt, "%%%d.%df", width, prec);
	sprintf(sout, fmt, val);

	return sout;
}

/******************************** StringSplit ********************************/

StringSplit::StringSplit(const String& str, const String& sep) :
	_Str(str),
	_Sep(sep)
{
	_Position	= 0;
	_Length		= 0;

	// 先算好第一段
	int p	= _Str.IndexOf(_Sep);
	if(p >= 0) _Length	= p;
}

const String StringSplit::Next()
{
	auto ptr	= _Str.GetBuffer();
	int len		= 0;

	if(_Position < 0 || _Length == 0)
		ptr	= nullptr;
	else
	{
		// 拿出当前段，然后提前计算下一段
		ptr	+= _Position;
		len	= _Length;

		// 找到下一个位置
		// 剩余全部长度，如果找不到下一个，那么这个就是最后长度
		int end	= _Str.Length();
		_Position += _Length + _Sep.Length();
		int p	= _Str.IndexOf(_Sep, _Position);
		if(p > 0) end	= p;

		_Length		= end - _Position;
	}

	// 包装一层指针
	return String((const char*)ptr, len);
}
