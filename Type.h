#ifndef __Type_H__
#define __Type_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 类型定义 */
typedef char            sbyte;
typedef unsigned char   byte;
typedef unsigned short  ushort;
typedef unsigned int    uint;
typedef unsigned long long  ulong;
typedef char*           string;
//typedef unsigned char   bool;
//#define true            1
//#define false           0

#define UInt64_Max 0xFFFFFFFFFFFFFFFFull

/*
// 尚未决定是否采用下面这种类型
typedef char            SByte;
typedef unsigned char   Byte;
typedef short           Int16;
typedef unsigned short  UInt16;
typedef int             Int32;
typedef unsigned int    UInt32;
typedef long long       Int64;
typedef unsigned long long  UInt64;
typedef char*           String;
*/
typedef long long       Int64;

#include <typeinfo>
using namespace ::std;

class String;
class Type;

// 根对象
class Object
{
private:
	//Type*	_Type;		// 类型

protected:

public:
	// 输出对象的字符串表示方式
	virtual String& ToStr(String& str) const;
	// 输出对象的字符串表示方式。支持RVO优化
	virtual String ToString() const;
	// 显示对象。默认显示ToString
	virtual void Show(bool newLine = false) const;

	//Type GetType() const;
};

/*// 类型
class Type
{
private:
	type_info* _info;

public:
	int		Size;	// 大小
	String	Name;	// 名称

	Type(type_info* ti);
};*/

// 数组长度
#define ArrayLength(arr) (sizeof(arr)/sizeof(arr[0]))
// 数组清零，固定长度
#define ArrayZero(arr) memset(arr, 0, ArrayLength(arr) * sizeof(arr[0]))
// 数组清零，可变长度
#define ArrayZero2(arr, len) memset(arr, 0, len * sizeof(arr[0]))
// 数组复制
#define ArrayCopy(dst, src) memcpy(dst, src, ArrayLength(src) * sizeof(src[0]))

template<typename T>
class IArray
{
public:
	virtual int Length() const = 0;
	virtual void SetAt(int i, const T& item) = 0;
	virtual T& operator[](int i) const = 0;
};

// 数组
/*
数组是指针和长度的封装。
设计本意：
1，数据处理函数无需同时传递指针和长度
2，数组下标越界检查，避免指针越界
3，数组自动扩容，避免因不知道函数内部需要多大缓存空间而带来的设计复杂度

按照指针来源可分为两大用法：
1，内部分配。随时可以扩容，对象拷贝时共用空间
2，外部指针。此时认为只是封装，不允许扩容

数组拷贝：
内=>内 共用数据区
内=>外 共用数据区
外=>内 仅复制指针
外=>外 仅复制指针
*/
template<typename T, int ArraySize = 0x40>
class Array : public Object, public IArray<T>
{
protected:
    T*		_Arr;		// 数据指针
	int		_Length;	// 数组长度
	uint	_Capacity;	// 数组最大容量
	bool	_needFree;	// 是否需要释放
	bool	_canWrite;	// 是否可写

	T		Arr[ArraySize];	// 内部缓冲区

	// 检查容量。如果不足则扩大，并备份指定长度的数据
	void CheckCapacity(int len, int bak = 0)
	{
		// 是否超出容量
		if(len <= _Capacity) return;

		// 自动计算合适的容量
		int k = 0x40;
		while(k < len) k <<= 1;

		T* p = new T[k];
		//ArrayZero2(p, _Capacity);

		// 是否需要备份数据
		if(bak > _Length) bak = _Length;
		if(bak > 0 && _Arr) memcpy(p, _Arr, bak);

		if(_needFree && _Arr) delete _Arr;

		_Capacity	= k;
		_Arr		= p;
		_needFree	= true;
	}

public:
	// 数组长度
    virtual int Length() const { return _Length; }
	// 数组最大容量。初始化时决定，后面不允许改变
	int Capacity() const { return _Capacity; }
	// 缓冲区
	T* GetBuffer() const { return _Arr; }

	// 初始化指定长度的数组。默认使用内部缓冲区
	Array(int length = ArraySize)
	{
		assert_param2(length <= 0x400, "禁止分配超过1k的数组");
		if(length < 0) length = ArrayLength(Arr);

		_Length		= length;
		if(length <= ArrayLength(Arr))
		{
			_Arr		= Arr;
			_Capacity	= ArrayLength(Arr);
			_needFree	= false;
		}
		else
		{
			_Arr		= new T[length];
			_Capacity	= length;
			_needFree	= true;
		}
		//ArrayZero2(_Arr, _Capacity);
		_canWrite	= true;
	}

	Array(const Array& arr)
	{
		_Length = arr.Length();
		_canWrite	= true;

		Copy(arr);
	}

	// 析构。释放资源
	virtual ~Array()
	{
		if(_needFree && _Arr) delete _Arr;
	}

	// 重载等号运算符，使用另一个固定数组来初始化
    Array& operator=(const Array& arr)
	{
		// 不要自己拷贝给自己
		if(&arr == this) return *this;

		_Length = 0;

		Copy(arr);

		return *this;
	}

	// 设置数组长度。容量足够则缩小Length，否则扩容以确保数组容量足够大避免多次分配内存
	bool SetLength(int length, bool bak = false)
	{
		if(length <= _Capacity)
		{
			_Length = length;
			return true;
		}
		else
		{
			CheckCapacity(length, bak ? _Length : 0);

			// 扩大长度
			if(length > _Length) _Length = length;

			return false;
		}
	}

	// 设置数组元素为指定值，自动扩容
	bool Set(const T& item, int index = 0, int count = 0)
	{
		assert_param2(_canWrite, "禁止修改数组数据");
		// count<=0 表示设置全部元素
		if(count <= 0) count = _Length - index;
		assert_param2(count > 0, "Array::Set的个数必须大于0");

		// 检查长度是否足够
		int len2 = index + count;
		CheckCapacity(len2, index);

		// 如果元素类型大小为1，那么可以直接调用内存设置函数
		if(sizeof(T) == 1)
			memset(_Arr + index, item, sizeof(T) * count);
		else
			while(count-- > 0) _Arr[index++] = item;

		// 扩大长度
		if(len2 > _Length) _Length = len2;

		return true;
	}

	// 设置数组。直接使用指针，不拷贝数据
	bool Set(T* data, int len = 0)
	{
		if(!Set((const T*)data, len)) return false;

		_canWrite	= true;

		return true;
	}

	// 设置数组。直接使用指针，不拷贝数据
	bool Set(const T* data, int len = 0)
	{
		int max	= len;
		if(!data)
		{
			data	= Arr;
			max		= ArrayLength(Arr);
		}
		// 自动计算长度，\0结尾
		else if(!len)
		{
			byte* p =(byte*)data;
			while(*p++) len++;
		}

		// 销毁旧的
		if(_needFree && _Arr) delete _Arr;

		_Arr		= (T*)data;
		_Length		= len;
		_Capacity	= max;
		_needFree	= false;
		_canWrite	= false;

		return true;
	}

	// 复制数组。深度克隆，拷贝数据
	int Copy(const Array& arr, int index = 0)
	{
		assert_param2(_canWrite, "禁止修改数组数据");
		if(&arr == this) return 0;
		if(arr.Length() == 0) return 0;

		return Copy(arr._Arr, arr.Length(), index);
	}

	// 复制数组。深度克隆，拷贝数据，自动扩容
	int Copy(const T* data, int len = 0, int index = 0)
	{
		assert_param2(_canWrite, "禁止修改数组数据");
		// 自动计算长度，\0结尾
		if(!len)
		{
			byte* p =(byte*)data;
			while(*p++) len++;
		}

		// 检查长度是否足够
		int len2 = index + len;
		CheckCapacity(len2, index);

		// 拷贝数据
		memcpy(_Arr + index, data, sizeof(T) * len);

		// 扩大长度
		if(len2 > _Length) _Length = len2;

		return len;
	}

	// 把当前数组复制到目标缓冲区。未指定长度len时复制全部
	int CopyTo(T* data, int len = 0, int index = 0) const
	{
		// 数据长度可能不足
		if(_Length - index < len || len == 0) len = _Length - index;
		if(len <= 0) return 0;

		// 拷贝数据
		memcpy(data, _Arr + index, sizeof(T) * len);

		return len;
	}

	// 清空已存储数据。
	virtual Array& Clear()
	{
		assert_param2(_canWrite, "禁止修改数组数据");
		memset(_Arr, 0, sizeof(T) * _Length);

		return *this;
	}

	// 设置指定位置的值，不足时自动扩容
	virtual void SetAt(int i, const T& item)
	{
		assert_param2(_canWrite, "禁止修改数组数据");
		// 检查长度，不足时扩容
		CheckCapacity(i + 1, _Length);

		if(i >= _Length) _Length = i + 1;

		_Arr[i] = item;
	}

	// 加入一个数据到末尾
	virtual int Push(T& item)
	{
		assert_param2(_canWrite, "禁止修改数组数据");

		int i = _Length;
		// 检查长度，不足时扩容
		CheckCapacity(i + 1, _Length);

		_Length++;

		_Arr[i] = item;

		return i;
	}

	// 末尾加入一个空数据，并返回引用，允许外部修改
	virtual T& Push()
	{
		assert_param2(_canWrite, "禁止修改数组数据");

		int i = _Length;
		// 检查长度，不足时扩容
		CheckCapacity(i + 1, _Length);

		_Length++;

		return _Arr[i];
	}

	// 弹出最后一个数组元素，长度减一
	virtual T& Pop()
	{
		assert_param2(_canWrite, "禁止修改数组数据");

		return _Arr[--_Length];
	}

    // 重载索引运算符[]，让它可以像数组一样使用下标索引。
    virtual T& operator[](int i) const
	{
		assert_param2(_Arr && i >= 0 && i < _Length, "数组下标越界");

		return _Arr[i];
	}

	friend bool operator==(const Array& bs1, const Array& bs2)
	{
		if(bs1.Length() != bs2.Length()) return false;

		for(int i=0; i<bs1.Length(); i++)
		{
			if(bs1[i] != bs2[i]) return false;
		}

		return true;
	}

	friend bool operator!=(const Array& bs1, const Array& bs2)
	{
		if(bs1.Length() != bs2.Length()) return true;

		for(int i=0; i<bs1.Length(); i++)
		{
			if(bs1[i] != bs2[i]) return true;
		}

		return false;
	}
};

// 字节数组
class ByteArray : public Array<byte>
{
public:
	ByteArray(int length = 0) : Array(length) { }
	ByteArray(byte item, int length) : Array(length) { Set(item, 0, length); }
	// 因为使用外部指针，这里初始化时没必要分配内存造成浪费
	ByteArray(const byte* data, int length, bool copy = false);
	ByteArray(byte* data, int length, bool copy = false);
	ByteArray(const ByteArray& arr) : Array(arr.Length()) { Copy(arr); }
	ByteArray(String& str);			// 直接引用数据缓冲区
	ByteArray(const String& str);	// 不允许修改，拷贝

	// 重载等号运算符，使用外部指针、内部长度，用户自己注意安全
    ByteArray& operator=(const byte* data);

	// 显示十六进制数据，指定分隔字符和换行长度
	String& ToHex(String& str, char sep = '-', int newLine = 0x10) const;
	// 显示十六进制数据，指定分隔字符和换行长度
	String ToHex(char sep = '-', int newLine = 0x10) const;

	// 保存到普通字节数组，首字节为长度
	int Load(const byte* data, int maxsize = -1);
	// 从普通字节数据组加载，首字节为长度
	int Save(byte* data, int maxsize = -1) const;

	// 输出对象的字符串表示方式
	virtual String& ToStr(String& str) const;
	// 显示对象。默认显示ToString
	virtual void Show(bool newLine = false) const;

	ushort	ToUInt16() const;
	uint	ToUInt32() const;
	ulong	ToUInt64() const;
	void Write(ushort value, int index = 0);
	void Write(short value, int index = 0);
	void Write(uint value, int index = 0);
	void Write(int value, int index = 0);
	void Write(ulong value, int index = 0);

    //friend bool operator==(const ByteArray& bs1, const ByteArray& bs2);
    //friend bool operator!=(const ByteArray& bs1, const ByteArray& bs2);
};

// 字符串
class String : public Array<char>
{
private:

public:
	// 字符串默认0长度，容量0x40
	String(int length = 0) : Array(length) { }
	String(char item, int count) : Array(count) { Set(item, 0, count); }
	// 因为使用外部指针，这里初始化时没必要分配内存造成浪费
	String(char* str, int len = 0) : Array(0) { Set(str, len); }
	String(const char* str, int len = 0) : Array(0) { Set(str, len); }
	String(const String& str) : Array(str.Length()) { Copy(str); }

	// 输出对象的字符串表示方式
	virtual String& ToStr(String& str) const;
	// 输出对象的字符串表示方式
	virtual String ToString() const;
	// 清空已存储数据。长度放大到最大容量
	virtual String& Clear();

	String& Append(char ch);
	String& Append(const char* str, int len = 0);
	String& Append(int value, int radix = 10, int width = 0);	// 写入整数，第二参数指定宽带，不足时补零
	String& Append(byte bt);		// 十六进制
	String& Append(ByteArray& bs);	// 十六进制

	// 调试输出字符串
	virtual void Show(bool newLine = false) const;

	// 格式化字符串，输出到现有字符串后面。方便我们连续格式化多个字符串
	String& Format(const char* format, ...);

    String& Concat(const Object& obj);
    String& Concat(const char* str, int len = 0);

    String& operator+=(const Object& obj);
    String& operator+=(const char* str);
    friend String& operator+(String& str, const Object& obj);
    friend String& operator+(String& str, const char* str2);
    friend String& operator+(String& str, char ch);
    friend String& operator+(String& str, byte bt);
    friend String& operator+(String& str, int value);
};

//String operator+(const char* str1, const char* str2);
String operator+(const char* str, const Object& obj);
String operator+(const Object& obj, const char* str);

#endif
