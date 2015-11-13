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
public:
	// 输出对象的字符串表示方式
	virtual String& ToStr(String& str) const;
	// 输出对象的字符串表示方式。支持RVO优化
	virtual String ToString() const;
	// 显示对象。默认显示ToString
	virtual void Show(bool newLine = false) const;

	const Type GetType() const;
};

// 类型
class Type
{
private:
	const type_info* _info;

	friend class Object;

	Type(const type_info* ti, int size);

public:
	int		Size;	// 大小
	//String	Name;	// 名称

	const String Name() const;	// 名称
};

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

// 数组。包括指针和最大长度，支持实际长度
class Array : public Object
{
protected:
    void*	_Arr;		// 数据指针
	int		_Length;	// 元素个数。非字节数
	uint	_Capacity;	// 最大个数。非字节数
	bool	_needFree;	// 是否需要释放
	bool	_canWrite;	// 是否可写
	ushort	_Size;		// 单个元素大小。字节

public:
	// 数组长度
    int Length() const;
	// 数组最大容量。初始化时决定，后面不允许改变
	int Capacity() const;
	// 缓冲区。按字节指针返回
	byte* GetBuffer() const;

	Array(void* data, int len = 0);
	Array(const void* data, int len = 0);
	// 重载等号运算符，使用另一个固定数组来初始化
    Array& operator=(const Array& arr);
	virtual ~Array();

	// 设置数组长度。容量足够则缩小Length，否则扩容以确保数组容量足够大避免多次分配内存
	bool SetLength(int length, bool bak = false);
	// 设置数组元素为指定值，自动扩容
	bool SetItem(const void* data, int index = 0, int count = 0);
	// 设置数组。直接使用指针，不拷贝数据
	bool Set(void* data, int len = 0);
	// 设置数组。直接使用指针，不拷贝数据
	bool Set(const void* data, int len = 0);
	// 复制数组。深度克隆，拷贝数据，自动扩容
	int Copy(const void* data, int len = 0, int index = 0);
	// 把当前数组复制到目标缓冲区。未指定长度len时复制全部
	int CopyTo(void* data, int len = 0, int index = 0) const;
	// 清空已存储数据。
	virtual void Clear();
	// 复制数组。深度克隆，拷贝数据
	int Copy(const Array& arr, int index = 0);
	// 设置指定位置的值，不足时自动扩容
	virtual void SetItemAt(int i, const void* item);

    // 重载索引运算符[]，返回指定元素的第一个字节
    byte& operator[](int i) const;

	friend bool operator==(const Array& bs1, const Array& bs2);
	friend bool operator!=(const Array& bs1, const Array& bs2);
protected:
	// 检查容量。如果不足则扩大，并备份指定长度的数据
	bool CheckCapacity(int len, int bak = 0);
	virtual void* Alloc(int len);
};

// 使用常量数组来定义一个指针数组
#define CArray(arr) (Array(arr, ArrayLength(arr)))
#define SArray(obj) (Array(&obj, sizeof(obj)))

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
class TArray : public Array, public IArray<T>
{
protected:
	T		Arr[ArraySize];	// 内部缓冲区

	virtual void* Alloc(int len) { return new T[len]; }

public:
	// 数组长度
    virtual int Length() const { return _Length; }
	// 缓冲区
	T* GetBuffer() const { return (T*)_Arr; }

	// 初始化指定长度的数组。默认使用内部缓冲区
	TArray(int length = ArraySize) : Array(Arr, ArrayLength(Arr))
	{
		assert_param2(length <= 0x400, "禁止分配超过1k的数组");
		if(length < 0) length = ArrayLength(Arr);

		_Length		= length;
		if(length > ArrayLength(Arr))
		{
			_Arr		= new T[length];
			_Capacity	= length;
			_needFree	= true;
		}

		_Size	= sizeof(T);
	}

	// 重载等号运算符，使用另一个固定数组来初始化
    TArray& operator=(const TArray& arr)
	{
		Array::operator=(arr);

		return *this;
	}

	// 让父类的所有Set函数在这里可见
	using Array::Set;

	// 设置数组元素为指定值，自动扩容
	bool Set(const T& item, int index = 0, int count = 0)
	{
		return SetItem(&item, index, count);
	}

	// 设置指定位置的值，不足时自动扩容
	virtual void SetAt(int i, const T& item)
	{
		SetItemAt(i, &item);
	}

	// 加入一个数据到末尾
	virtual int Push(const T& item)
	{
		SetItemAt(_Length, &item);

		return _Length;
	}

	// 末尾加入一个空数据，并返回引用，允许外部修改
	virtual T& Push()
	{
		assert_param2(_canWrite, "禁止修改数组数据");

		int i = _Length;
		// 检查长度，不足时扩容
		CheckCapacity(i + 1, _Length);

		_Length++;

		T* buf = (T*)_Arr;
		return buf[i];
	}

	// 弹出最后一个数组元素，长度减一
	virtual T& Pop()
	{
		assert_param2(_canWrite, "禁止修改数组数据");

		T* buf = (T*)_Arr;
		return buf[--_Length];
	}

	// 查找元素
	virtual int FindIndex(const T item)
	{
		T* buf = (T*)_Arr;
		for(int i=0; i<Length(); i++)
		{
			if(buf[i] == item) return i;
		}

		return -1;
	}

    // 重载索引运算符[]，让它可以像数组一样使用下标索引。
    virtual T& operator[](int i) const
	{
		assert_param2(_Arr && i >= 0 && i < _Length, "数组下标越界");

		T* buf = (T*)_Arr;
		return buf[i];
	}
};

// 字节数组
class ByteArray : public TArray<byte>
{
public:
	ByteArray(int length = 0) : TArray(length) { }
	ByteArray(byte item, int length) : TArray(length) { Set(item, 0, length); }
	// 因为使用外部指针，这里初始化时没必要分配内存造成浪费
	ByteArray(const void* data, int length, bool copy = false);
	ByteArray(void* data, int length, bool copy = false);
	ByteArray(const ByteArray& arr) : TArray(arr.Length()) { Copy(arr); }
	ByteArray(String& str);			// 直接引用数据缓冲区
	ByteArray(const String& str);	// 不允许修改，拷贝

	// 重载等号运算符，使用外部指针、内部长度，用户自己注意安全
    ByteArray& operator=(const void* data);

	// 显示十六进制数据，指定分隔字符和换行长度
	String& ToHex(String& str, char sep = '-', int newLine = 0x10) const;
	// 显示十六进制数据，指定分隔字符和换行长度
	String ToHex(char sep = '-', int newLine = 0x10) const;

	// 保存到普通字节数组，首字节为长度
	int Load(const void* data, int maxsize = -1);
	// 从普通字节数据组加载，首字节为长度
	int Save(void* data, int maxsize = -1) const;

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
class String : public TArray<char>
{
private:

public:
	// 字符串默认0长度，容量0x40
	String(int length = 0) : TArray(length) { }
	String(char item, int count) : TArray(count) { Set(item, 0, count); }
	// 因为使用外部指针，这里初始化时没必要分配内存造成浪费
	String(void* str, int len = 0) : TArray(0) { Set(str, len); }
	String(const void* str, int len = 0) : TArray(0) { Set(str, len); }
	String(const String& str) : TArray(str.Length()) { Copy(str); }

	// 输出对象的字符串表示方式
	virtual String& ToStr(String& str) const;
	// 输出对象的字符串表示方式
	virtual String ToString() const;
	// 清空已存储数据。长度放大到最大容量
	virtual void Clear();

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

// 从数组创建列表
#define MakeList(T, arr) List<T>(&arr[0], sizeof(arr)/sizeof(arr[0]))

// 数组
class JArray
{
private:
    void**	_Arr;
	int		_Count;
	int		_Capacity;

public:
	// 有效元素个数
    int Count() const { return _Count; }
	// 最大元素个数
    int Capacity() const { return _Capacity; }

	JArray(int capacity = 0x10)
	{
		_Capacity = capacity;
		_Count = 0;

		_Arr = new void*[capacity];
		ArrayZero2(_Arr, capacity);
	}

	~JArray()
	{
		if(_Arr) delete _Arr;
		_Arr = NULL;
	}

	// 压入一个元素
	int Push(void* item)
	{
		assert_param(_Count < _Capacity);

		// 找到空闲位放置
		int idx = _Count++;
		_Arr[idx] = item;

		return idx;
	}

	// 弹出一个元素
	const void* Pop()
	{
		assert_param(_Count > 0);

		return _Arr[--_Count];
	}

    // 重载索引运算符[]，让它可以像数组一样使用下标索引。
    void* operator[](int i)
	{
		assert_param(i >= 0 && i < _Count);
		return _Arr[i];
	}
	// 列表转为指针，注意安全
    //T* operator=(TArray arr) { return arr.Arr; }
};

// 固定大小的指针数组。
/*
用于数据元素不多，有个上限的场合。主要为了方便遍历。
存储数据时，从头开始找到空位来存放，不用时清空相应空位。
所以需要注意的是，在数组里面存储的元素不一定连续。
*/
template<typename T, int ArraySize>
class FixedArray
{
private:
	uint	_Count;
	T*		Arr[ArraySize];

public:
	// 默认初始化。大小和数组元素全部清零
	FixedArray()
	{
		_Count = 0;
		ArrayZero(Arr);
	}

	// 使用另一个固定数组来初始化
	FixedArray(FixedArray& arr)
	{
		_Count = arr._Count;
		ArrayCopy(Arr, arr.Arr);
	}

	// 重载等号运算符，使用另一个固定数组来初始化
    FixedArray& operator=(FixedArray& arr)
	{
		_Count = arr._Count;
		ArrayCopy(Arr, arr.Arr);

		return *this;
	}

	~FixedArray() { DeleteAll(); }

	// 实际元素个数
	uint Count() const { return _Count; }

	// 数组总长度
	uint Length() const { return ArraySize; }

	// 压入一个元素。返回元素所存储的索引
	int Add(T* item)
	{
		assert_ptr(item);
		//assert_param(_Count < ArraySize);

		if(_Count >= ArraySize) return -1;

		// 找到空闲位放置
		int i = 0;
		for(; i < ArraySize && Arr[i]; i++);
		// 检查是否还有空位
		if(i >= ArraySize) return -1;

		Arr[i] = item;
		_Count++;

		return i;
	}

	// 弹出最后一个元素
	T* Pop()
	{
		if(_Count == 0) return NULL;

		// 找到最后一个元素
		int i = ArraySize - 1;
		for(; i >=0 && !Arr[i]; i--);

		T* item = Arr[i];
		_Count--;
		Arr[i] = NULL;

		return item;
	}

	// 删除元素
	bool Remove(T* item)
	{
		int idx = Find(item);
		if(idx < 0) return false;

		RemoveAt(idx);

		return true;
	}

	// 删除指定位置的元素
	void RemoveAt(int idx)
	{
		assert_param(idx >= 0 && idx < ArraySize);

		_Count--;
		Arr[idx] = NULL;
	}

	// 清空数组。个数设0，所有元素清零
	FixedArray& Clear()
	{
		_Count = 0;
		ArrayZero(Arr);

		return *this;
	}

	// 释放所有指针指向的内存
	FixedArray& DeleteAll()
	{
		for(int i=0; i < ArraySize; i++)
		{
			if(Arr[i]) delete Arr[i];
		}

		return *this;
	}

	// 查找元素
	int Find(T* item)
	{
		assert_ptr(item);

		int i = 0;
		for(; i < ArraySize && Arr[i] != item; i++);
		if(i >= ArraySize) return -1;

		return i;
	}

	// 移到下一个元素，累加参数，如果没有下一个元素则返回false。
	bool MoveNext(int& idx)
	{
		//assert_ptr(idx);

		// 从idx开始，找到第一个非空节点。注意，存储不一定连续
		int i = idx + 1;
		for(; i < ArraySize && !Arr[i]; i++);
		if(i >= ArraySize) return false;

		idx = i;

		return true;
	}

    // 重载索引运算符[]，让它可以像数组一样使用下标索引。
    /*T*& operator[](int i)
	{
		assert_param(i >= 0 && i < ArraySize);
		return Arr[i];
	}*/
	// 不能允许[]作为左值，否则外部直接设置数据以后，Count并没有改变
    T* operator[](int i)
	{
		assert_param(i >= 0 && i < ArraySize);
		return Arr[i];
	}
};

/*// 变长列表模版
// T一般是指针，列表内部有一个数组用于存放指针
template<typename T>
class List : public TArray<T*>
{
public:
    List(int size = 0) : TArray(size) { _Length = 0; }
    List(T* items, uint count) { Set(items, count); }

	// 添加单个元素
    virtual void Add(const T& item) { Push(item); }

	// 添加多个元素
    void Add(T* items, int count)
    {
        for(int i=0; i<count; i++) Push(*items++);
    }

	// 删除指定位置元素
	void RemoveAt(int index)
	{
		int len = Length();
		if(len <= 0 || index >= len) return;

		// 复制元素
		if(index < len - 1) memmove(&_Arr[index], &_Arr[index + 1], (len - index - 1) * sizeof(T));
		_Length--;
	}

	// 删除指定元素
	virtual void Remove(const T& item)
	{
		int index = FindIndex(item);
		if(index >= 0) RemoveAt(index);
	}

	// 释放所有指针指向的内存
	List& DeleteAll()
	{
		for(int i=0; i < Length(); i++)
		{
			if(_Arr[i]) delete _Arr[i];
		}

		return *this;
	}

	virtual void Clear() { _Length = 0; }

	// 返回内部指针
    const T* ToArray() { return _Arr; }
};*/

// 双向链表
template <class T> class LinkedList;

// 双向链表节点。实体类继承该类
template <class T> class LinkedNode
{
	// 友元类。允许链表类控制本类私有成员
    friend class LinkedList<T>;

public:
    T* Prev;	// 上一个节点
    T* Next;	// 下一个节点

    void Initialize()
    {
        Next = NULL;
        Prev = NULL;
    }

    // 从链表中删除。需要修改前后节点的指针指向，但当前节点仍然指向之前的前后节点
    void RemoveFromList()
    {
        if(Prev) Prev->Next = Next;
        if(Next) Next->Prev = Prev;
    }

	// 完全脱离链表。不再指向其它节点
    void Unlink()
    {
        if(Prev) Prev->Next = Next;
        if(Next) Next->Prev = Prev;

        Next = NULL;
        Prev = NULL;
    }

	// 把当前节点附加到另一个节点之后
	void LinkAfter(T* node)
	{
		node->Next = (T*)this;
		Prev = node;
		// 不能清空Next，因为可能是两个链表的合并
		//Next = NULL;
	}

	// 最后一个节点
	T* Last()
	{
		T* node = (T*)this;
		while(node->Next) node = node->Next;

		return node;
	}

	// 附加到当前节点后面
	void Append(T* node)
	{
		Next = node;
		node->Prev = (T*)this;
		//node->Next = NULL;
	}
};

// 双向链表
template <class T>
class LinkedList
{
public:
    // 链表节点。实体类不需要继承该内部类
	class Node
	{
	public:
		T		Item;	// 元素
		Node*	Prev;	// 前一节点
		Node*	Next;	// 下一节点

		Node()
		{
			Prev = NULL;
			Next = NULL;
		}

		// 从队列中脱离
		void RemoveFromList()
		{
			// 双保险，只有在前后节点指向当前节点时，才修改它们的值
			if(Prev && Prev->Next == this) Prev->Next = Next;
			if(Next && Next->Prev == this) Next->Prev = Prev;
		}

		// 附加到当前节点后面
		void Append(Node* node)
		{
			Next = node;
			node->Prev = this;
		}
	};

private:
	Node*	_Head;		// 链表头部
    Node*	_Tail;		// 链表尾部
	int 	_Count;		// 元素个数

	void Init()
	{
        _Head = NULL;
		_Tail = NULL;
		_Count = 0;
	}

	void Remove(Node* node)
	{
		// 脱离队列
		node->RemoveFromList();
		// 特殊处理头尾
		if(node == _Head) _Head = node->Next;
		if(node == _Tail) _Tail = node->Prev;

		delete node;
		_Count--;
	}

public:
    LinkedList()
    {
		Init();
    }

	// 计算链表节点数
    virtual int Count() const { return _Count; }

	// 将某项添加到集合
	virtual void Add(const T& item)
	{
		Node* node = new Node();
		node->Item = item;

		if(_Tail)
			_Tail->Append(node);
		else
			_Head = _Tail = node;

		_Count++;
	}

	// 从集合中移除特定对象的第一个匹配项
	virtual void Remove(const T& item)
	{
		if(!_Count) return;

		Node* node;
		for(node = _Head; node; node = node->Next)
		{
			if(node->Item == item) break;
		}

		if(node) Remove(node);
	}

	// 确定集合是否包含特定值
	virtual bool Contains(const T& item)
	{
		if(!_Count) return false;

		Node* node;
		for(node = _Head; node; node = node->Next)
		{
			if(node->Item == item) return true;
		}

		return false;
	}

	// 从集合中移除所有项。注意，该方法不会释放元素指针
	virtual void Clear()
	{
		if(!_Count) return;

		Node* node;
		for(node = _Head; node;)
		{
			// 先备份，待会删除可能影响指针
			Node* next = node->Next;
			delete node;
			node = next;
		}

		Init();
	}

	// 将集合元素复制到数组中
	virtual void CopyTo(T* arr)
	{
		assert_ptr(arr);

		if(!_Count) return;

		Node* node;
		for(node = _Head; node; node = node->Next)
		{
			*arr++ = node->Item;
		}
	}

	T& First() { return _Head->Item; }
	T& Last() { return _Tail->Item; }

	// 释放第一个有效节点
    T& ExtractFirst()
    {
		if(!_Count) return NULL;

        Node* node = _Head;
        _Head = _Head->Next;
		// 可能只有一个节点
		if(!_Head)
			_Tail = NULL;
		else
			_Head->Prev = NULL;

		T& item = node->Item;
		delete node;
		_Count--;

        return item;
    }

	// 释放最后一个有效节点
    T& ExtractLast()
    {
		if(!_Count) return NULL;

        Node* node = _Tail;
        _Tail = _Tail->Prev;
		// 可能只有一个节点
		if(!_Tail)
			_Head = NULL;
		else
			_Tail->Next = NULL;

		T& item = node->Item;
		delete node;
		_Count--;

        return item;
    }
};

void* operator new(uint size);
void* operator new[](uint size);
void operator delete(void* p);
void operator delete [] (void* p);

//#define DEBUG_NEW new(__FILE__, __LINE__)
//#define new DEBUG_NEW
//#endif

// 自动释放的智能指针
class SmartPtr
{
private:
	// 内部包装。指针的多个智能指针SmartPtr对象共用该内部包装
	class WrapPtr
	{
	public:
		void*	Ptr;	// 目标指针
		uint	Count;	// 引用计数

		WrapPtr(void* ptr) { Ptr = ptr; Count = 1; }
		~WrapPtr() { delete Ptr; }
	};
	WrapPtr* _ptr;

public:
	// 为某个指针封装
	SmartPtr(void* ptr)
	{
		assert_ptr(ptr);

		_ptr = new WrapPtr(ptr);
	}

	// 拷贝智能指针。仅拷贝内部包装，然后引用计数加一
	SmartPtr(const SmartPtr& ptr)
	{
		assert_ptr(ptr._ptr);
		assert_ptr(ptr._ptr->Ptr);

		_ptr = ptr._ptr;
		_ptr->Count++;
	}

	~SmartPtr()
	{
		// 释放智能指针时，指针引用计数减一
		_ptr->Count--;
		// 如果指针引用计数为0，则释放指针
		if(!_ptr->Count)
		{
			delete _ptr;
			_ptr = NULL;
		}
	}

	void* ToPtr() { return _ptr->Ptr; }
};

// 经典的C++自动指针
// 超出对象作用域时自动销毁被管理指针
template<class T>
class auto_ptr
{
private:
	T* _ptr;

public:
	// 普通指针构造自动指针，隐式转换
	// 构造函数的explicit关键词有效阻止从一个“裸”指针隐式转换成auto_ptr类型
	explicit auto_ptr(T* p = 0) : _ptr(p) { }
	// 拷贝构造函数，解除原来自动指针的管理权
	auto_ptr(auto_ptr& ap) : _ptr(ap.release()) { }
	// 析构时销毁被管理指针
	~auto_ptr()
	{
		// 因为C++保证删除一个空指针是安全的，所以我们没有必要判断空
		/*if(_ptr)
		{
			delete _ptr;
			_ptr = NULL;
		}*/
		delete _ptr;
	}

	// 自动指针拷贝，解除原来自动指针的管理权
	auto_ptr& operator=(T* p)
	{
		_ptr = p;
		return *this;
	}

	// 自动指针拷贝，解除原来自动指针的管理权
	auto_ptr& operator=(auto_ptr& ap)
	{
		reset(ap.release());
		return *this;
	}

	// 获取原始指针
	T* get() const { return _ptr; }

	// 重载*和->运算符
	T& operator*() const { assert_param(_ptr); return *_ptr; }
	T* operator->() const { return _ptr; }

	// 接触指针的管理权
	T* release()
	{
		T* p = _ptr;
		_ptr = 0;
		return p;
	}

	// 销毁原指针，管理新指针
	void reset(T* p = 0)
	{
		if(_ptr != p)
		{
			//if(_ptr) delete _ptr;
			// 因为C++保证删除一个空指针是安全的，所以我们没有必要判断空
			delete _ptr;
			_ptr = p;
		}
	}
};

#endif
