#ifndef _List_H_
#define _List_H_

// 变长列表。仅用于存储指针
class List
{
public:
    explicit List();
    List(const List& list);
    List(List&& list);
	~List();

	int Count() const;

	// 添加单个元素
    void Add(void* item);

	// 添加多个元素
    void Add(void** items, uint count);

	// 删除指定位置元素
	void RemoveAt(uint index);

	// 删除指定元素
	int Remove(const void* item);

	void Clear();

	// 查找指定项。不存在时返回-1
	int FindIndex(const void* item) const;

	// 释放所有指针指向的内存
	List& DeleteAll();

    // 重载索引运算符[]，返回指定元素的第一个
    void* operator[](int i) const;
    void*& operator[](int i);

#if DEBUG
	static void Test();
#endif

private:
	void**	_Arr;
	uint	_Count;
	uint	_Capacity;

	void*	Arr[0x10];

	void Init();
	bool CheckCapacity(int count);
};

#endif
