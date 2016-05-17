#ifndef _List_H_
#define _List_H_

// 变长列表
class List
{
public:
    List(int count = 0);
    //List(void* items, uint count);

	int Count() const;
	
	// 添加单个元素
    void Add(void* item);

	// 添加多个元素
    void Add(void* items, int count);

	// 删除指定位置元素
	void RemoveAt(int index);

	// 删除指定元素
	void Remove(const void* item);

	void Clear();

	// 释放所有指针指向的内存
	List& DeleteAll();

	// 返回内部指针
    //const void** ToArray() const;

private:
	void**	_Arr;
	int		_Count;
	int		_Capacity;

	void*	Arr[0x10];
};

#endif
