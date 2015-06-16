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

#include <typeinfo>
using namespace ::std;

// 根对象。
// 子类通过Init重写指定大小，并对虚表指针以外数据区域全部清零。因无法确定其它虚表指针位置，故不支持多继承
class Object
{
private:
	int		_Size;		// 当前对象内存大小

protected:
	// 初始化为指定大小，并对虚表指针以外数据区域全部清零。不支持多继承
	void Init(int size);

public:
	// 子类重载，可直接使用OBJECT_INIT宏定义，为了调用Init(size)初始化大小并清零
	virtual void Init() { };

	// 输出对象的字符串表示方式
	virtual const char* ToString();
};

// 子类用于调用Object进行对象初始化的默认写法
#define OBJECT_INIT virtual void Init() { Object::Init(sizeof(this[0])); }

// 数组长度
#define ArrayLength(arr) sizeof(arr)/sizeof(arr[0])
// 数组清零，固定长度
#define ArrayZero(arr) memset(arr, 0, ArrayLength(arr) * sizeof(arr[0]))
// 数组清零，可变长度
#define ArrayZero2(arr, len) memset(arr, 0, len * sizeof(arr[0]))
// 数组复制
#define ArrayCopy(dst, src) memcpy(dst, src, ArrayLength(src) * sizeof(src[0]))

// 数组
template<typename T>
class Array : public Object
{
private:
    T*		_Arr;
	int		_Count;
	int		_Capacity;
	bool	_needFree;

	T		Arr[0x40];	// 内部缓冲区，减少内存分配次数

	OBJECT_INIT

	void InitCapacity(int capacity)
	{
		if(capacity <= ArrayLength(Arr))
		{
			_Capacity = ArrayLength(Arr);
			_Arr = Arr;
		}
		else
		{
			_Capacity = capacity;
			_Arr = new T[capacity];
			ArrayZero2(_Arr, capacity);
			_needFree = true;
		}
	}

public:
	// 有效元素个数
    int Count() const { return _Count; }
	// 最大元素个数
    int Capacity() const { return _Capacity; }
	// 缓冲区
	T* GetBuffer() { return _Arr; }

	Array(int capacity = 0x40)
	{
		Init();

		InitCapacity(capacity);
	}

	Array(T item, int count)
	{
		Init();

		_Count		= count;

		InitCapacity(count);

		if((byte)item != 0) memset(_Arr, item, count);
	}

	Array(const T* data, int len = 0)
	{
		// 自动计算长度，\0结尾
		if(!len)
		{
			byte* p =(byte*)data;
			while(*p++) len++;
		}

		//memcpy(_Arr, data, len * sizeof(T));
		_Count		= len;
		_Capacity	= len;
		_Arr		= (T*)data;
		_needFree	= false;
	}

	Array(Array& arr)
	{
		_Count		= arr._Count;
		_Capacity	= arr._Capacity;
		//ArrayCopy(_Arr, arr._Arr);
		_Arr		= arr._Arr;
		_needFree	= arr._needFree;
	}

	// 重载等号运算符，使用另一个固定数组来初始化
    Array& operator=(Array& arr)
	{
		_Count		= arr._Count;
		_Capacity	= arr._Capacity;
		//ArrayCopy(_Arr, arr._Arr);
		_Arr		= arr._Arr;
		_needFree	= arr._needFree;

		return *this;
	}

	~Array()
	{
		if(_needFree)
		{
			if(_Arr) delete _Arr;
			_Arr = NULL;
		}
	}

	// 压入一个元素
	int Push(T item)
	{
		assert_param(_Count < _Capacity);

		// 找到空闲位放置
		int idx = _Count++;
		_Arr[idx] = item;

		return idx;
	}

	// 弹出一个元素
	const T Pop()
	{
		assert_param(_Count > 0);

		return _Arr[--_Count];
	}

	// 附加一组数据
	void Append(const T* data, int len)
	{
		assert_param(_Count + len <= _Capacity);

		// 找到空闲位放置
		while(len-- > 0) _Arr[_Count++] = *data++;
	}

	// 清空已存储数据
	void Clear()
	{
		// 调整使用内部存储
		if(_needFree)
		{
			if(_Arr) delete _Arr;
		}
		_Arr = Arr;
		_Capacity = ArrayLength(Arr);
		if(_Count > 0)
		{
			memset(_Arr, 0, _Count * sizeof(T));
		}
		_Count = 0;
	}

    // 重载索引运算符[]，让它可以像数组一样使用下标索引。
    T& operator[](int i)
	{
		assert_param(i >= 0 && i < _Count);

		return _Arr[i];
	}
	// 列表转为指针，注意安全
    //T* operator=(Array arr) { return arr.Arr; }
};

class String;

// 字节数组
class ByteArray : public Array<byte>
{
public:
	ByteArray(int capacity = 0x40) : Array(capacity) { }
	ByteArray(byte item, int count) : Array(item, count) { }
	ByteArray(String& str);

	// 列表转为指针，注意安全
    //byte* operator=(ByteArray& arr) { return arr.GetBuffer(); }

	// 显示十六进制数据，指定分隔字符和换行长度
	String& ToHex(String& str, char sep = '\0', int newLine = 0x10);
};

// 字符串
class String : public Array<char>
{
private:

	OBJECT_INIT

public:
	String(int capacity = 0x40) : Array(capacity) { }
	String(char item, int count) : Array(item, count) { }
	String(const char* data, int len = 0) : Array(data, len) { }
	String(String& str) : Array(str) { }
	// 重载等号运算符，使用另一个固定数组来初始化
    //String& operator=(String& str);
	//~String();

    // 重载索引运算符[]，让它可以像数组一样使用下标索引。
    //char& operator[](int i);

	// 输出对象的字符串表示方式
	virtual const char* ToString();

	String& Append(char ch);
	String& Append(const char* str);

	// 调试输出字符串
	void Show(bool newLine = false);
};

#endif
