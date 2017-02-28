#ifndef _Array_H_
#define _Array_H_

#include "Buffer.h"

// 变长数组。自动扩容
class Array : public Buffer
{
public:
	bool	Expand;	// 是否可扩容
	
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

#if DEBUG
	static void Test();
#endif

protected:
	int		_Capacity;	// 最大个数。非字节数
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

#endif
