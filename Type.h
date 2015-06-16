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

// 根对象
class Object
{
private:
	int		_Size;		// 当前对象内存大小
	//int		_Ref;		// 引用计数

public:
	Object(int size);

	virtual const char* ToString();
};

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
class Array : Object
{
private:
    T*		_Arr;
	int		_Count;
	int		_Capacity;

public:
	// 有效元素个数
    int Count() const { return _Count; }
	// 最大元素个数
    int Capacity() const { return _Capacity; }

	Array(int capacity = 0x10) : Object(sizeof(this))
	{
		_Capacity = capacity;
		_Count = 0;

		_Arr = new T[capacity];
		ArrayZero2(_Arr, capacity);
	}

	Array(T* data, int len = 0) : Object(sizeof(this))
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
		_Arr		= data;
	}

	Array(Array& arr) : Object(sizeof(this))
	{
		_Count		= arr._Count;
		_Capacity	= arr._Capacity;
		ArrayCopy(_Arr, arr._Arr);
	}

	// 重载等号运算符，使用另一个固定数组来初始化
    Array& operator=(Array& arr)
	{
		_Count		= arr._Count;
		_Capacity	= arr._Capacity;
		ArrayCopy(_Arr, arr._Arr);

		return *this;
	}

	~Array()
	{
		if(_Arr) delete _Arr;
		_Arr = NULL;
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

    // 重载索引运算符[]，让它可以像数组一样使用下标索引。
    T operator[](int i)
	{
		assert_param(i >= 0 && i < _Count);

		return _Arr[i];
	}
	// 列表转为指针，注意安全
    //T* operator=(Array arr) { return arr.Arr; }
};

// 字节数组
class ByteArray : Array<byte>
{
};

// 字符串
class String : Object
{
private:
	int		_Count;
    char*	_Arr;

public:
	// 有效元素个数
    int Count() const { return _Count; }
	char* GetBuffer() { return _Arr; }

	String(const char* data, int len = 0);
	String(String& str);
	// 重载等号运算符，使用另一个固定数组来初始化
    String& operator=(String& str);
	~String();

    // 重载索引运算符[]，让它可以像数组一样使用下标索引。
    char operator[](int i);
};

#endif
