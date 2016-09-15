#ifndef __String_H__
#define __String_H__

#include "Array.h"
#include "ByteArray.h"

// 字符串助手，主要用于字符串连接
class StringSplit;

// 字符串
class String : public Array
{
public:
	String(cstring cstr = "");
	String(const String& str);
	String(String&& rval);
	// 外部传入缓冲区供内部使用，内部计算字符串长度，注意长度减去零结束符
	String(char* str, int length);
	String(char* str, int length, bool expand);
	// 包装静态字符串，直接使用，修改时扩容
	String(cstring str, int length);
	explicit String(char c);
	explicit String(byte value, int radix = 10);
	explicit String(short value, int radix = 10);
	explicit String(ushort value, int radix = 10);
	explicit String(int value, int radix = 10);
	explicit String(uint value, int radix = 10);
	explicit String(Int64 value, int radix = 10);
	explicit String(UInt64 value, int radix = 10);
	explicit String(float value, int decimalPlaces = 4);
	explicit String(double value, int decimalPlaces = 8);
	//virtual ~String();

	using Array::SetLength;
	using Array::Copy;

	// 内存管理
	//inline uint Length() const { return _Length; }
	inline cstring GetBuffer() const { return (cstring)_Arr; }
	// 设置数组长度。改变长度后，确保最后以0结尾
	virtual bool SetLength(int length, bool bak);

	// 拷贝数据，默认-1长度表示当前长度
	virtual int Copy(int destIndex, const void* src, int len);
	// 拷贝数据，默认-1长度表示两者最小长度
	//virtual int Copy(int destIndex, const Buffer& src, int srcIndex, int len);
	// 把数据复制到目标缓冲区，默认-1长度表示当前长度
	virtual int CopyTo(int srcIndex, void* dest, int len) const;

	// 为被赋值对象建立一个备份。
	// 如果值为空或无效，或者内存分配失败，字符串将会被标记为无效
	String& operator = (const String& rhs);
	String& operator = (cstring cstr);
	String& operator = (String&& rval);

	// 连接内建类型。如果参数无效则认为连接失败
	bool Concat(const Object& obj);
	bool Concat(const String& str);
	bool Concat(cstring cstr);
	bool Concat(char c);
	bool Concat(byte c, int radix = 10);
	bool Concat(short num, int radix = 10);
	bool Concat(ushort num, int radix = 10);
	bool Concat(int num, int radix = 10);
	bool Concat(uint num, int radix = 10);
	bool Concat(Int64 num, int radix = 10);
	bool Concat(UInt64 num, int radix = 10);
	bool Concat(float num, int decimalPlaces = 4);
	bool Concat(double num, int decimalPlaces = 8);

	/*template<typename T>
	String& operator += (T rhs)	{Concat(rhs); return (*this);}*/
	String& operator += (const Object& rhs)	{Concat(rhs); return (*this);}
	String& operator += (const String& rhs)	{Concat(rhs); return (*this);}
	String& operator += (cstring cstr)	{Concat(cstr); return (*this);}
	String& operator += (char c)			{Concat(c); return (*this);}
	String& operator += (byte num)			{Concat(num); return (*this);}
	String& operator += (int num)			{Concat(num); return (*this);}
	String& operator += (uint num)			{Concat(num); return (*this);}
	String& operator += (Int64 num)			{Concat(num); return (*this);}
	String& operator += (UInt64 num)		{Concat(num); return (*this);}
	String& operator += (float num)			{Concat(num); return (*this);}
	String& operator += (double num)		{Concat(num); return (*this);}

	friend String& operator + (String& lhs, const Object& rhs);
	friend String& operator + (String& lhs, const String& rhs);
	friend String& operator + (String& lhs, cstring cstr);
	friend String& operator + (String& lhs, char c);
	friend String& operator + (String& lhs, byte num);
	friend String& operator + (String& lhs, int num);
	friend String& operator + (String& lhs, uint num);
	friend String& operator + (String& lhs, Int64 num);
	friend String& operator + (String& lhs, UInt64 num);
	friend String& operator + (String& lhs, float num);
	friend String& operator + (String& lhs, double num);

    explicit operator bool() const { return _Length > 0; }
    bool operator !() const { return _Length == 0; }
	//operator char*() const { return _Arr; }
	int CompareTo(const String& s) const;
	int CompareTo(cstring cstr, int len = -1, bool ignoreCase = false) const;
	bool Equals(const String& s) const;
	bool Equals(cstring cstr) const;
	bool EqualsIgnoreCase(const String& s) const;
	bool EqualsIgnoreCase(cstring cstr) const;
	bool operator == (const String& rhs) const	{return Equals(rhs);	}
	bool operator == (cstring cstr) const 		{return Equals(cstr);	}
	bool operator != (const String& rhs) const	{return !Equals(rhs);	}
	bool operator != (cstring cstr) const		{return !Equals(cstr);	}
	bool operator <  (const String& rhs) const;
	bool operator >  (const String& rhs) const;
	bool operator <= (const String& rhs) const;
	bool operator >= (const String& rhs) const;

	//void SetAt(int index, char c);
	char operator [] (int index) const;
	char& operator [] (int index);
	void GetBytes(byte* buf, int bufsize, int index=0) const;
	ByteArray GetBytes() const;
	ByteArray ToHex() const;
	void ToArray(char* buf, int bufsize, int index=0) const { GetBytes((byte*)buf, bufsize, index); }

	int ToInt() const;
	float ToFloat() const;
	double ToDouble() const;

	// 输出对象的字符串表示方式
	virtual String& ToStr(String& str) const;
	// 输出对象的字符串表示方式
	virtual String ToString() const;
	//// 清空已存储数据。
	//virtual void Clear();

	// 调试输出字符串
	virtual void Show(bool newLine = false) const;

	// 格式化字符串，输出到现有字符串后面。方便我们连续格式化多个字符串
	String& Format(cstring format, ...);

	int IndexOf(const char ch, int startIndex = 0) const;
	int IndexOf(const String& str, int startIndex = 0) const;
	int IndexOf(cstring str, int startIndex = 0) const;
	int LastIndexOf(const char ch, int startIndex = 0) const;
	int LastIndexOf(const String& str, int startIndex = 0) const;
	int LastIndexOf(cstring str, int startIndex = 0) const;
	bool Contains(const String& str) const;
	bool Contains(cstring str) const;
	bool StartsWith(const String& str, int startIndex = 0) const;
	bool StartsWith(cstring str, int startIndex = 0) const;
	bool EndsWith(const String& str) const;
	bool EndsWith(cstring str) const;

	StringSplit Split(const String& sep) const;
	StringSplit Split(cstring sep) const;

	String Substring(int start, int len = -1) const;
	String TrimStart() const;
	String TrimEnd() const;
	String Trim() const;
	String Replace(char find, char replace) const;
	String Replace(const String& find, const String& replace) const;
	String Remove(int index) const;
	String Remove(int index, int count) const;
	String ToLower() const;
	String ToUpper() const;

	// 静态比较器。比较两个字符串指针
	static int Compare(const void* v1, const void* v2);

#if DEBUG
	static void Test();
#endif

private:
	//char*	_Arr;		// 字符数组
	//int		_Capacity;	// 容量，不包含0结束符
	//int		_Length;		// 字符串长度，不包含0结束符
	//bool	_needFree;	// 是否需要释放
	//bool	_canWrite;	// 是否可写

	char	Arr[0x40];

	void init();
	void release();
	bool Concat(cstring cstr, uint length);

	String& copy(cstring cstr, uint length);
	void move(String& rhs);
	bool CopyOrWrite();

	using Array::CheckCapacity;
	bool CheckCapacity(uint size);
	virtual void* Alloc(int len);

	int Search(cstring str, int len, int startIndex, bool rev) const;
};

#define R(str) String(str)

class StringSplit
{
public:
	cstring Sep;		// 分隔符。下一个要寻找的边界符
	int		Position;	// 当前段位置。负数表示到了结尾。
	int		Length;		// 当前段长度。

	StringSplit(const String& str, cstring sep);

	const String Next();

	// 是否已经到达末尾
	//bool End() const { return Position == -2; }

    explicit operator bool() const { return Position >= -1; }
    bool operator !() const { return Position < -1; }

private:
	const String& _Str;
};

#endif
