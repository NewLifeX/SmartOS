#include "Type.h"
#include "Buffer.h"
#include "List.h"

IList::IList()
{
	Init();
}

IList::IList(const IList& list)
{
	Init();

	// 如果list的缓冲区是自己的，则拷贝过来
	// 如果不是自己的，则直接拿过来用
	if(list._Arr != list.Arr) CheckCapacity(list._Count);

	_Count	= list._Count;
	Comparer	= list.Comparer;

	Buffer(_Arr, _Count << 2)	= list._Arr;
}

IList::IList(IList&& list)
{
	_Count	= list._Count;
	_Capacity	= list._Capacity;
	Comparer	= list.Comparer;

	// 如果list的缓冲区是自己的，则拷贝过来
	// 如果不是自己的，则直接拿过来用
	if(list._Arr == list.Arr)
	{
		_Arr	= Arr;
		Buffer(_Arr, _Count << 2)	= list._Arr;
	}
	else
	{
		_Arr	= list._Arr;
		list.Init();
	}
}

IList::~IList()
{
	if(_Arr && _Arr != Arr) delete _Arr;
}

void IList::Init()
{
	_Arr	= Arr;
	_Count	= 0;
	_Capacity	= ArrayLength(Arr);

	Comparer	= nullptr;
}

//int IList::Count() const { return _Count; }

// 添加单个元素
void IList::Add(void* item)
{
	CheckCapacity(_Count + 1);

	_Arr[_Count++]	= item;
}

// 添加多个元素
void IList::Add(void** items, uint count)
{
	if(!items || !count) return;

	CheckCapacity(_Count + count);

	while(count--) _Arr[_Count++]	= items++;
}

// 删除指定位置元素
void IList::RemoveAt(uint index)
{
	int len = _Count;
	if(len <= 0 || index >= len) return;

	// 复制元素，最后一个不用复制
	int remain	= len - 1 - index;
	if(remain)
	{
		len	= remain * sizeof(void*);
		Buffer(&_Arr[index], len).Copy(0, &_Arr[index + 1], len);
	}
	_Count--;
}

// 删除指定元素
int IList::Remove(const void* item)
{
	int idx = FindIndex(item);
	if(idx >= 0) RemoveAt(idx);

	return idx;
}

int IList::FindIndex(const void* item) const
{
	for(int i=0; i<_Count; i++)
	{
		if(_Arr[i] == item) return i;
		if(Comparer && Comparer(_Arr[i], item) == 0) return i;
	}

	return -1;
}

// 释放所有指针指向的内存
IList& IList::DeleteAll()
{
	for(int i=0; i < _Count; i++)
	{
		if(_Arr[i]) delete (int*)_Arr[i];
	}

	return *this;
}

void IList::Clear()
{
	_Count = 0;
}

// 重载索引运算符[]，返回指定元素的第一个
void* IList::operator[](int i) const
{
	if(i<0 || i>=_Count) return nullptr;

	return _Arr[i];
}

void*& IList::operator[](int i)
{
	if(i<0 || i>=_Count)
	{
		static void* dummy;
		return dummy;
	}

	return _Arr[i];
}

bool IList::CheckCapacity(int count)
{
	// 是否超出容量
	if(_Arr && count <= _Capacity) return true;

	// 自动计算合适的容量
	int sz = 0x40 >> 2;
	while(sz < count) sz <<= 1;

	void* p = new byte[sz << 2];
	if(!p) return false;

	// 需要备份数据
	if(_Count > 0 && _Arr)
		// 为了安全，按照字节拷贝
		Buffer(p, sz << 2).Copy(0, _Arr, _Count << 2);

	if(_Arr && _Arr != Arr) delete _Arr;

	_Arr		= (void**)p;
	_Capacity	= sz;

	return true;
}
