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

class String;
class Type;

// 根对象。
// 子类通过Init重写指定大小，并对虚表指针以外数据区域全部清零。因无法确定其它虚表指针位置，故不支持多继承
class Object
{
private:
	//Type*	_Type;		// 类型

protected:
	// 初始化为指定大小，并对虚表指针以外数据区域全部清零。不支持多继承
	void Init(int size);

public:
	// 子类重载，可直接使用OBJECT_INIT宏定义，为了调用Init(size)初始化大小并清零
	virtual void Init() { };

	// 输出对象的字符串表示方式
	virtual const char* ToString();

	virtual String& To(String& str);

	void Show();

	//Type GetType();
};

/*class Type
{
private:
	type_info* _info;

public:
	int		Size;	// 大小
	String	Name;	// 名称
};*/

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
protected:
    T*		_Arr;		// 数据指针
	int		_Length;	// 数组长度
	uint	_Capacity;	// 数组最大容量。初始化时决定，后面不允许改变
	bool	_needFree;	// 是否需要释放

	T		Arr[0x40];	// 内部缓冲区，减少内存分配次数

	OBJECT_INIT

	// 释放外部缓冲区。使用最大容量的内部缓冲区
	void Release()
	{
		if(_needFree && _Arr && _Arr != Arr) delete _Arr;
		_Arr		= Arr;
		_Length		= ArrayLength(Arr);
		_Capacity	= ArrayLength(Arr);
		_needFree	= false;
	}

public:
	// 数组长度
    int Length() const { return _Length; }
	// 数组最大容量。初始化时决定，后面不允许改变
	int Capacity() const { return _Capacity; }
	// 缓冲区
	T* GetBuffer() { return _Arr; }

	// 初始化指定长度的数组。默认使用内部缓冲区
	Array(int length = ArrayLength(Arr))
	{
		Init();

		if(!length) length = ArrayLength(Arr);
		_Length		= length;
		// 超过内部默认缓冲区大小时，另外从堆分配空间
		if(length <= ArrayLength(Arr))
		{
			_Arr		= Arr;
			_Capacity	= ArrayLength(Arr);
			ArrayZero2(_Arr, _Capacity);
		}
		else
		{
			_Arr = new T[length];
			_Capacity	= length;
			_needFree	= true;
			ArrayZero2(_Arr, length);
		}
	}

	// 析构。释放资源
	virtual ~Array() { Release(); }

	// 重载等号运算符，使用另一个固定数组来初始化
    Array& operator=(const Array& arr) { return Copy(arr); }

	// 设置数组元素为指定值
	Array& Set(T item, int index = 0, int count = 0)
	{
		// count==0 表示设置全部元素
		if(!count) count = _Length - index;

		// 检查长度是否足够
		assert_param(index + count <= _Length);

		// 如果元素类型大小为1，那么可以直接调用内存设置函数
		if(sizeof(T) == 1)
			memset(_Arr + index, item, sizeof(T) * count);
		else
			while(count-- > 0) _Arr[index++] = item;

		return *this;
	}

	// 设置数组。采用浅克隆，不拷贝数据
	Array& Set(const T* data, int len = 0)
	{
		// 自动计算长度，\0结尾
		if(!len)
		{
			byte* p =(byte*)data;
			while(*p++) len++;
		}

		// 检查长度是否足够
		//assert_param(len <= _Capacity);

		// 销毁旧的
		Release();

		_Arr		= (T*)data;
		_Length		= len;
		_Capacity	= len;
		_needFree	= false;

		return *this;
	}

	// 复制数组。深度克隆，拷贝数据
	Array& Copy(const Array& arr, int index = 0)
	{
		int len = arr.Length();

		// 检查长度是否足够
		assert_param(index + len <= _Capacity);

		// 拷贝数据
		memcpy(_Arr + index, arr._Arr, sizeof(T) * len);
		_Length = len;

		return *this;
	}

	// 复制数组。深度克隆，拷贝数据
	Array& Copy(const T* data, int len = 0, int index = 0)
	{
		// 自动计算长度，\0结尾
		if(!len)
		{
			byte* p =(byte*)data;
			while(*p++) len++;
		}

		// 检查长度是否足够
		int len2 = index + len;
		assert_param(len2 <= _Capacity);

		// 拷贝数据
		memcpy(_Arr + index, data, sizeof(T) * len);
		if(len2 > _Length) _Length = len2;

		return *this;
	}

	// 清空已存储数据。长度放大到最大容量
	virtual Array& Clear()
	{
		_Length = _Capacity;

		memset(_Arr, 0, sizeof(T) * _Length);

		return *this;
	}

    // 重载索引运算符[]，让它可以像数组一样使用下标索引。
    T& operator[](int i)
	{
		assert_param(_Arr && i >= 0 && i < _Length);

		return _Arr[i];
	}
};

// 字节数组
class ByteArray : public Array<byte>
{
public:
	ByteArray(int length) : Array(length) { }
	ByteArray(byte item, int length) : Array(length) { Set(item, 0, length); }
	// 因为使用外部指针，这里初始化时没必要分配内存造成浪费
	ByteArray(const byte* data, int length) : Array(0) { Set(data, length); }
	ByteArray(String& str);

	// 显示十六进制数据，指定分隔字符和换行长度
	String& ToHex(String& str, char sep = '\0', int newLine = 0x10);
};

// 字符串
class String : public Array<char>
{
private:

	OBJECT_INIT

public:
	String(int length = 0x40) : Array(length) { }
	String(char item, int count) : Array(count) { Set(item, 0, count); }
	// 因为使用外部指针，这里初始化时没必要分配内存造成浪费
	String(const char* str, int len = 0) : Array(0) { Set(str, len); }
	String(String& str) : Array(str.Length()) { Copy(str); }

	// 设置字符串长度
	String& SetLength(int length);
	
	// 输出对象的字符串表示方式
	virtual const char* ToString();
	// 清空已存储数据。长度放大到最大容量
	virtual String& Clear();

	String& Append(char ch);
	String& Append(const char* str, int len = 0);
	String& Append(int value);
	String& Append(byte bt);		// 十六进制
	String& Append(ByteArray bs);	// 十六进制

	// 调试输出字符串
	void Show(bool newLine = false);

	String& Format(const char* format, ...);
};

// IP地址
class IPAddress : public Object
{
public:
	uint	Value;	// 地址

	IPAddress(int value)		{ Value = (uint)value; }
	IPAddress(uint value = 0)	{ Value = value; }
	IPAddress(IPAddress& addr)	{ Value = addr.Value; }
	IPAddress(const byte* ips)	{ Value = *(uint*)ips; }
	IPAddress(byte ip1, byte ip2, byte ip3, byte ip4);

    IPAddress& operator=(int v)			{ Value = (uint)v; return *this; }
    IPAddress& operator=(uint v)		{ Value = v; return *this; }
    IPAddress& operator=(const byte* v)	{ Value = *(uint*)v; return *this; }

    // 重载索引运算符[]，让它可以像数组一样使用下标索引。
    byte& operator[](int i);

	// 字节数组
    byte* ToArray();

	bool IsAny();
	bool IsBroadcast();
	uint GetSubNet(IPAddress& mask);	// 获取子网

	virtual String& To(String& str);

    friend bool operator==(IPAddress& addr1, IPAddress& addr2) { return addr1.Value == addr2.Value; }
    friend bool operator!=(IPAddress& addr1, IPAddress& addr2) { return addr1.Value != addr2.Value; }

	static IPAddress Any;
	static IPAddress Broadcast;
};

// IP结点
class IPEndPoint : public Object
{
public:
	IPAddress	Address;	// 地址
	ushort		Port;		// 端口

	IPEndPoint();
	IPEndPoint(IPAddress& addr, ushort port);

	virtual String& To(String& str);

    friend bool operator==(IPEndPoint& addr1, IPEndPoint& addr2)
	{
		return addr1.Port == addr2.Port && addr1.Address == addr2.Address;
	}
    friend bool operator!=(IPEndPoint& addr1, IPEndPoint& addr2)
	{
		return addr1.Port != addr2.Port || addr1.Address != addr2.Address;
	}
};

// Mac地址。结构体和类都可以
//typedef struct _MacAddress MacAddress;
//struct _MacAddress
class MacAddress : public Object
{
public:
	// 长整型转为Mac地址，取内存前6字节。因为是小字节序，需要v4在前，v2在后
	// 比如Mac地址12-34-56-78-90-12，v4是12-34-56-78，v2是90-12，ulong是0x0000129078563412
	//uint	v4;
	//ushort	v2;
	ulong	Value;	// 地址

	MacAddress(ulong v = 0);

	// 是否广播地址，全0或全1
	bool IsBroadcast();

    MacAddress& operator=(ulong v);

    // 重载索引运算符[]，让它可以像数组一样使用下标索引。
    byte& operator[](int i);

	// 字节数组
    byte* ToArray();

	virtual String& To(String& str);

    friend bool operator==(MacAddress& addr1, MacAddress& addr2)
	{
		//return addr1.v4 == addr2.v4 && addr1.v2 == addr2.v2;
		return addr1.Value == addr2.Value;
	}
    friend bool operator!=(MacAddress& addr1, MacAddress& addr2)
	{
		//return addr1.v4 != addr2.v4 || addr1.v2 != addr2.v2;
		return addr1.Value != addr2.Value;
	}

	static MacAddress Empty;
	static MacAddress Full;
};
//}MacAddress;

//#define IP_FULL 0xFFFFFFFF
//#define MAC_FULL 0xFFFFFFFFFFFFFFFFull

#endif
