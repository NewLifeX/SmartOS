#ifndef _List_H_
#define _List_H_

typedef int (*IComparer)(const void* v1, const void* v2);

// 变长列表。仅用于存储指针
class IList
{
public:
    IComparer	Comparer;	// 比较器

	IList();
    IList(const IList& list);
    IList(IList&& list);
	virtual ~IList();

	IList& operator=(const IList& list);
	IList& operator=(IList&& list);

	inline int Count()	const { return _Count; }

	// 添加单个元素
    void Add(void* item);

	// 添加多个元素
    void Add(void** items, int count);

	// 删除指定位置元素
	void RemoveAt(int index);

	// 删除指定元素
	int Remove(const void* item);

	void Clear();

	// 查找指定项。不存在时返回-1
	int FindIndex(const void* item) const;

	// 释放所有指针指向的内存
	IList& DeleteAll();

    // 重载索引运算符[]，返回指定元素的第一个
    void* operator[](int i) const;
    void*& operator[](int i);

#if DEBUG
	static void Test();
#endif

private:
	void**	_Arr;
	int		_Count;
	int		_Capacity;

	void*	Arr[0x04];

	void Init();
	bool CheckCapacity(int count);
	void move(IList& list);
};

template<typename T>
class List : public IList
{
	static_assert(sizeof(T) <= 4, "List only support pointer or int");
public:
	virtual ~List() { };

	// 添加单个元素
    void Add(T item) { IList::Add((void*)item); }

	// 删除指定元素
	int Remove(const T item) { return IList::Remove((const void*)item); }

	// 查找指定项。不存在时返回-1
	int FindIndex(const T item) const { return IList::FindIndex((const void*)item); }

    // 重载索引运算符[]，返回指定元素的第一个
    T operator[](int i) const	{ return (T)IList::operator[](i); }
    T& operator[](int i)		{ return (T&)IList::operator[](i); }
};

#endif
