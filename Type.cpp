#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
#include "Sys.h"
#include "Platform\stm32.h"

/******************************** Object ********************************/

// 输出对象的字符串表示方式
String& Object::ToStr(String& str) const
{
	const char* name = typeid(*this).name();
	while(*name >= '0' && *name <= '9') name++;

	str.Set(name, 0);

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
	// 为了减少堆分配，采用较大的栈缓冲区
	char cs[0x200];
	String str(cs, ArrayLength(cs));
	str.SetLength(0);
	ToStr(str);
	str.Show(newLine);
}

const Type Object::GetType() const
{
	int* p = (int*)this;

	return Type(&typeid(*this), *(p - 1));
}

/******************************** Type ********************************/

Type::Type(const type_info* ti, int size)
{
	_info	= ti;

	Size	= size;
}

const String Type::Name() const
{
	const char* name = _info->name();
	while(*name >= '0' && *name <= '9') name++;

	return String(name);
}

/******************************** TArray ********************************/
// 数组长度
int Array::Length() const { return _Length; }
// 数组最大容量。初始化时决定，后面不允许改变
int Array::Capacity() const { return _Capacity; }
// 缓冲区。按字节指针返回
byte* Array::GetBuffer() const { return (byte*)_Arr; }

int MemLen(const void* data)
{
	if(!data) return 0;

	// 自动计算长度，\0结尾，单字节大小时才允许
	int len = 0;
	const byte* p =(const byte*)data;
	while(*p++) len++;
	return len;
}

Array::Array(void* data, int len)
{
	_Size	= 1;

	if(!len) len = MemLen(data);

	_Arr		= data;
	_Length		= len;
	_Capacity	= len;
	_needFree	= false;
	_canWrite	= true;
}

Array::Array(const void* data, int len)
{
	_Size	= 1;

	if(!len) len = MemLen(data);

	_Arr		= (void*)data;
	_Length		= len;
	_Capacity	= len;
	_needFree	= false;
	_canWrite	= false;
}

// 析构。释放资源
Array::~Array()
{
	if(_needFree && _Arr) delete _Arr;
}

// 重载等号运算符，使用另一个固定数组来初始化
Array& Array::operator=(const Array& arr)
{
	// 不要自己拷贝给自己
	if(&arr == this) return *this;

	_Length = 0;

	Copy(arr);

	return *this;
}

// 重载等号运算符，使用外部指针、内部长度，用户自己注意安全
/*Array& Array::operator=(const void* data)
{
	if(Length() > 0) Copy(data, Length());

	return *this;
}*/

// 设置数组长度。容量足够则缩小Length，否则扩容以确保数组容量足够大避免多次分配内存
bool Array::SetLength(int length, bool bak)
{
	if(length <= _Capacity)
	{
		_Length = length;
	}
	else
	{
		if(!CheckCapacity(length, bak ? _Length : 0)) return false;
		// 扩大长度
		if(length > _Length) _Length = length;
	}
	return true;
}

// 设置数组元素为指定值，自动扩容
bool Array::SetItem(const void* data, int index, int count)
{
	assert_param2(_canWrite, "禁止SetItem修改");
	assert_param2(data, "Array::SetItem data Error");

	// count<=0 表示设置全部元素
	if(count <= 0) count = _Length - index;
	assert_param2(count > 0, "Array::SetItem count Error");

	// 检查长度是否足够
	int len2 = index + count;
	CheckCapacity(len2, index);

	byte* buf = (byte*)GetBuffer();
	// 如果元素类型大小为1，那么可以直接调用内存设置函数
	if(_Size == 1)
		memset(&buf[index], *(byte*)data, count);
	else
	{
		while(count-- > 0)
		{
			memcpy(buf, data, _Size);
			buf += _Size;
		}
	}

	// 扩大长度
	if(len2 > _Length) _Length = len2;

	return true;
}

// 设置数组。直接使用指针，不拷贝数据
bool Array::Set(void* data, int len)
{
	if(!Set((const void*)data, len)) return false;

	_canWrite	= true;

	return true;
}

// 设置数组。直接使用指针，不拷贝数据
bool Array::Set(const void* data, int len)
{
	if(!len) len = MemLen(data);

	// 销毁旧的
	if(_needFree && _Arr && _Arr != data) delete _Arr;

	_Arr		= (void*)data;
	_Length		= len;
	_Capacity	= len;
	_needFree	= false;
	_canWrite	= false;

	return true;
}

// 复制数组。深度克隆，拷贝数据，自动扩容
int Array::Copy(const void* data, int len, int index)
{
	assert_param2(_canWrite, "禁止CopyData修改");
	assert_param2(data, "Array::Copy data Error");

	if(!len) len = MemLen(data);

	// 检查长度是否足够
	int len2 = index + len;
	CheckCapacity(len2, index);

	// 拷贝数据
	memcpy((byte*)_Arr + _Size * index, data, _Size * len);

	// 扩大长度
	if(len2 > _Length) _Length = len2;

	return len;
}

// 复制数组。深度克隆，拷贝数据
int Array::Copy(const Array& arr, int index)
{
	assert_param2(_canWrite, "禁止CopyArray修改数据");

	if(&arr == this) return 0;
	if(arr.Length() == 0) return 0;

	return Copy(arr._Arr, arr.Length(), index);
}

// 把当前数组复制到目标缓冲区。未指定长度len时复制全部
int Array::CopyTo(void* data, int len, int index) const
{
	assert_param2(data, "Array::CopyTo data Error");

	// 数据长度可能不足
	if(_Length - index < len || len <= 0) len = _Length - index;
	if(len <= 0) return 0;

	// 拷贝数据
	memcpy(data, (byte*)_Arr + _Size * index, _Size * len);

	return len;
}

// 清空已存储数据。
void Array::Clear()
{
	assert_param2(_canWrite, "禁止Clear修改");
	assert_param2(_Arr, "Clear数据不能为空指针");

	memset(_Arr, 0, _Size * _Length);
}

// 设置指定位置的值，不足时自动扩容
void Array::SetItemAt(int i, const void* item)
{
	assert_param2(_canWrite, "禁止SetItemAt修改");

	// 检查长度，不足时扩容
	CheckCapacity(i + 1, _Length);

	if(i >= _Length) _Length = i + 1;

	//_Arr[i] = item;
	memcpy((byte*)_Arr + _Size * i, item, _Size);
}

// 重载索引运算符[]，返回指定元素的第一个字节
byte& Array::operator[](int i) const
{
	assert_param2(_Arr && i >= 0 && i < _Length, "下标越界");

	byte* buf = (byte*)_Arr;
	if(_Size > 1) i *= _Size;

	return buf[i];
}

// 检查容量。如果不足则扩大，并备份指定长度的数据
bool Array::CheckCapacity(int len, int bak)
{
	// 是否超出容量
	if(len <= _Capacity) return true;

	// 自动计算合适的容量
	int k = 0x40;
	while(k < len) k <<= 1;

	void* p = Alloc(k);
	if(!p) return false;

	// 是否需要备份数据
	if(bak > _Length) bak = _Length;
	if(bak > 0 && _Arr) memcpy(p, _Arr, bak);

	if(_needFree && _Arr && _Arr != p) delete _Arr;

	_Capacity	= k;
	_Arr		= p;
	_needFree	= true;

	return true;
}

void Array::Show(bool newLine) const
{
	ByteArray bs(GetBuffer(), Length());
	bs.Show(newLine);
}

void* Array::Alloc(int len) { return new byte[_Size * len];}

bool operator==(const Array& bs1, const Array& bs2)
{
	if(bs1.Length() != bs2.Length()) return false;

	return memcmp(bs1._Arr, bs2._Arr, bs1.Length() * bs1._Size) == 0;
}

bool operator!=(const Array& bs1, const Array& bs2)
{
	if(bs1.Length() != bs2.Length()) return true;

	return memcmp(bs1._Arr, bs2._Arr, bs1.Length() * bs1._Size) != 0;
}

/*void* Array::Set(void* data, byte dat, int count)
{
	return memset(data, dat, count);
}

void* Array::Copy(void* dst, const void* src, int count)
{
	if(!dst || !src || dst == src || !count) return dst;

	// 如果区域重叠，要用memmove
	if(dst < src && dst + count > src
	|| dst > src && src + count > dst)
		return memmove(dst, src, count);
	else
		return memcpy(dst, src, count);
}*/

/******************************** ByteArray ********************************/

ByteArray::ByteArray(const void* data, int length, bool copy) : TArray(0)
{
	if(copy)
		Copy(data, length);
	else
		Set(data, length);
}

ByteArray::ByteArray(void* data, int length, bool copy) : TArray(0)
{
	if(copy)
		Copy(data, length);
	else
		Set(data, length);
}

// 字符串转为字节数组
ByteArray::ByteArray(String& str) : TArray(0)
{
	char* p = str.GetBuffer();
	Set((byte*)p, str.Length());
}

// 不允许修改，拷贝
ByteArray::ByteArray(const String& str) : TArray(0)
{
	const char* p = str.GetBuffer();
	//Copy((const byte*)p, str.Length());
	Set((const byte*)p, str.Length());
}

// 重载等号运算符，使用外部指针、内部长度，用户自己注意安全
/*ByteArray& ByteArray::operator=(const void* data)
{
	if(Length() > 0) Copy(data, Length());

	return *this;
}*/

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

	//return ToHex(str, sep, newLine);

	// 优化为使用RVO
	ToHex(str, sep, newLine);

	return str;
}

// 保存到普通字节数组，首字节为长度
int ByteArray::Load(const void* data, int maxsize)
{
	const byte* p = (const byte*)data;
	_Length = p[0] <= maxsize ? p[0] : maxsize;

	return Copy(p + 1, _Length);
}

// 从普通字节数据组加载，首字节为长度
int ByteArray::Save(void* data, int maxsize) const
{
	assert_param(_Length <= 0xFF);

	byte* p = (byte*)data;
	int len = _Length <= maxsize ? _Length : maxsize;
	p[0] = len;

	return CopyTo(p + 1, len);
}

String& ByteArray::ToStr(String& str) const
{
	return ToHex(str, '-', 0x20);
}

// 显示对象。默认显示ToString
void ByteArray::Show(bool newLine) const
{
	// 采用栈分配然后复制，避免堆分配
	char cs[0x200];
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
void String::Clear()
{
	TArray::Clear();

	_Length = 0;
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

String& String::Append(const ByteArray& bs)
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
	obj.ToStr(*this);

	return *this;
}

String& String::Concat(const char* str, int len)
{
	Copy(str, 0, Length());

	return *this;
}

int String::IndexOf(const String& str) const
{
	if(str.Length() > Length()) return -1;

	auto ptr	= GetBuffer();
	auto ptr2	= str.GetBuffer();
	if(ptr[0] != ptr2[0]) return -1;

	auto p	= strstr(ptr, ptr2);
	if(!p) return -1;

	return p - ptr;
}

int String::IndexOf(const char* str) const
{
	uint len	= MemLen(str);
	if(len > Length()) return -1;

	auto ptr	= GetBuffer();
	if(str[0] != ptr[0]) return -1;

	auto p	= strstr(ptr, str);
	if(!p) return -1;

	return p - ptr;
}

String String::Sub(int start, int len) const
{
	String str;
	//str.Copy(this, len, start);
	str.Copy(GetBuffer() + start, len);

	return str;
}

String String::TrimStart() const
{
	String str;

	auto ptr	= GetBuffer();
	// 找到第一个不是空白字符的位置
	int i = 0;
	for(;i < Length(); i++)
	{
		auto ch	= ptr[i];
		if(ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') break;
	}
	str.Copy(ptr + i, Length() - i);

	return str;
}

String String::TrimEnd() const
{
	String str;

	auto ptr	= GetBuffer();
	// 找到最后一个不是空白字符的位置
	int i = Length() - 1;
	for(;i >= 0; i--)
	{
		auto ch	= ptr[i];
		if(ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') break;
	}
	str.Copy(ptr, i + 1);

	return str;
}

String String::Trim() const
{
	//String str	= TrimStart();

	return TrimStart().TrimEnd();;
}

bool String::StartsWith(const String& str) const { return Sub(0, str.Length()) == str; }
bool String::StartsWith(const char* str) const { return Sub(0, strlen(str)) == str; }
bool String::EndsWith(const String& str) const { return Sub(Length() - str.Length(), str.Length()) == str; }
bool String::EndsWith(const char* str) const { return Sub(Length() - strlen(str), strlen(str)) == str; }

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
	// 要把字符串拷贝过来
	String s;
	s = str;
	s += obj;
	return s;
}

String operator+(const Object& obj, const char* str)
{
	// 要把字符串拷贝过来
	String s;
	obj.ToStr(s);
	s += str;
	return s;
}

String& operator+(String& str, char ch) { return str.Append(ch); }
String& operator+(String& str, byte bt) { return str.Append(bt); }
String& operator+(String& str, int value) { return str.Append(value); }

bool operator==(const String& str, const String& str2)
{
	return str.Length() == str2.Length() && str.IndexOf(str2) == 0;
}

bool operator!=(const String& str, const String& str2)
{
	return str.Length() != str2.Length() || str.IndexOf(str2) != 0;
}

bool operator==(const String& str, const char* str2)
{
	return str.Length() == MemLen(str2) && str.IndexOf(str2) == 0;
}

bool operator!=(const String& str, const char* str2)
{
	return str.Length() != MemLen(str2) || str.IndexOf(str2) != 0;
}

/******************************** REV ********************************/


//extern uint32_t __REV(uint32_t value);
//#ifdef STM32F0
//extern uint32_t __REV16(uint32_t value);
//#else
//extern uint32_t __REV16(uint16_t value);
//#endif

uint	_REV(uint value)		{ return __REV(value); }
ushort	_REV16(ushort value)	{ return __REV16(value); }
