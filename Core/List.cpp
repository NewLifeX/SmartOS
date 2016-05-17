#include "Type.h"
#include "Buffer.h"
#include "List.h"

List::List(int size)
{
	_Count	= 0;
}

//List::List(T* items, uint count) { Set(items, count); }

int List::Count() const { return _Count; }

// 添加单个元素
void List::Add(void* item)
{
	//Push(item);
}

/*// 添加多个元素
void List::Add(T* items, int count)
{
	for(int i=0; i<count; i++) Push(*items++);
}*/

// 删除指定位置元素
void List::RemoveAt(int index)
{
	int len = _Count;
	if(len <= 0 || index >= len) return;

	// 复制元素
	//if(index < len - 1) memmove(&_Arr[index], &_Arr[index + 1], (len - index - 1) * sizeof(T));
	if(index < len - 1)
	{
		len	= (len - index - 1) * sizeof(void*);
		Buffer(&_Arr[index], len).Copy(0, &_Arr[index + 1], len);
	}
	_Count--;
}

// 删除指定元素
void List::Remove(const void* item)
{
	//int index = FindIndex(item);
	//if(index >= 0) RemoveAt(index);
}

// 释放所有指针指向的内存
List& List::DeleteAll()
{
	for(int i=0; i < _Count; i++)
	{
		if(_Arr[i]) delete _Arr[i];
	}

	return *this;
}

void List::Clear()
{
	_Count = 0;
}

/*// 返回内部指针
const void** List::ToArray() const
{
	return _Arr;
}*/
