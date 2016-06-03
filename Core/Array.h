#ifndef _Array_H_
#define _Array_H_

#include "Buffer.h"

// 变长数组。自动扩容
class Array : public Buffer
{
public:
	// 数组最大容量。初始化时决定，后面不允许改变
	inline int Capacity() const { return _Capacity; }

	Array(void* data, int len);
	Array(const void* data, int len);
	Array(const Array& rhs) = delete;
	Array(Array&& rval);
	explicit Array(const Buffer& rhs);

	virtual ~Array();

	Array& operator = (const Buffer& rhs);
	Array& operator = (const void* p);
	Array& operator = (Array&& rval);

	using Buffer::Set;
	using Buffer::SetLength;
	using Buffer::Copy;

	// 设置数组长度。容量足够则缩小Length，否则扩容以确保数组容量足够大避免多次分配内存
	virtual bool SetLength(int len);
	virtual bool SetLength(int len, bool bak);
	//virtual void SetBuffer(void* ptr, int len);
	//virtual void SetBuffer(const void* ptr, int len);
	// 拷贝数据，默认-1长度表示使用右边最大长度，左边不足时自动扩容
	virtual int Copy(int destIndex, const Buffer& src, int srcIndex, int len);

	// 设置数组元素为指定值，自动扩容
	bool SetItem(const void* data, int index, int count);
	// 设置数组。直接使用指针，不拷贝数据
	bool Set(void* data, int len);
	// 设置数组。直接使用指针，不拷贝数据
	bool Set(const void* data, int len);
	// 清空已存储数据。
	virtual void Clear();
	// 设置指定位置的值，不足时自动扩容
	virtual void SetItemAt(int i, const void* item);

    // 重载索引运算符[]，返回指定元素的第一个字节
    byte operator[](int i) const;
    byte& operator[](int i);

	friend bool operator==(const Array& bs1, const Array& bs2);
	friend bool operator!=(const Array& bs1, const Array& bs2);

protected:
	uint	_Capacity;	// 最大个数。非字节数
	bool	_needFree;	// 是否需要释放
	bool	_canWrite;	// 是否可写
	ushort	_Size;		// 单个元素大小。字节

	void Init();
	void move(Array& rval);

	// 检查容量。如果不足则扩大，并备份指定长度的数据
	bool CheckCapacity(int len, int bak);
	virtual void* Alloc(int len);
	// 释放已占用内存
	virtual bool Release();
};

// 使用常量数组来定义一个指针数组
//#define CArray(arr) (Array(arr, ArrayLength(arr)))
//#define SArray(obj) (Array(&obj, sizeof(obj)))

// 使用常量数组来定义一个指针数组
//#define CBuffer(arr) (Buffer(arr, ArrayLength(arr)))
//#define SBuffer(obj) (Buffer(&obj, sizeof(obj)))

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
*/
template<typename T, int ArraySize = 0x40>
class TArray : public Array
{
protected:
	T		Arr[ArraySize];	// 内部缓冲区

	virtual void* Alloc(int len)
	{
		if(len <= ArraySize)
		{
			_needFree	= false;
			return Arr;
		}
		else
		{
			_needFree	= true;
			return new T[len];
		}
	}

public:
	// 数组长度
    virtual int Length() const { return _Length; }
	// 缓冲区
	T* GetBuffer() const { return (T*)_Arr; }

	// 初始化指定长度的数组。默认使用内部缓冲区
	TArray(int length = ArraySize) : Array(Arr, ArrayLength(Arr))
	{
		//assert(length <= 0x400, "禁止分配超过1k的数组");
		if(length < 0) length = ArrayLength(Arr);

		_Length		= length;
		if(length > ArrayLength(Arr))
		{
			_Arr		= (char*)new T[length];
			_Capacity	= length;
			_needFree	= true;
		}

		_Size	= sizeof(T);
	}

	TArray& operator=(const TArray& arr) = delete;

	// 让父类的所有Set函数在这里可见
	using Array::Set;

	// 设置数组元素为指定值，自动扩容
	bool Set(const T& item, int index, int count)
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
		//assert(_canWrite, "禁止修改");

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
		//assert(_canWrite, "禁止修改");

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
		//assert(_Arr && i >= 0 && i < _Length, "下标越界");

		T* buf = (T*)_Arr;
		return buf[i];
	}
};

#endif
