#ifndef _List_H_
#define _List_H_

#include <stddef.h>
#include "Sys.h"

// 数组长度
#define ArrayLength(arr) sizeof(arr)/sizeof(arr[0])
// 从数组创建列表
#define MakeList(T, arr) List<T>(&arr[0], sizeof(arr)/sizeof(arr[0]))

// 定长数组模版
template<typename T, int array_size>
__packed class Array
{
private:
	int _Count;
    T Arr[array_size];

public:
	// 有效元素个数
    int Count() { return _Count; }
	// 最大元素个数
    int Max() { return array_size; }

	void Init()
	{
		_Count = 0;
		memset(Arr, 0x00, array_size * sizeof(T));
	}

	// 压入一个元素
	int Push(const T& item)
	{
		assert_param(_Count < array_size);

		// 找到空闲位放置
		int idx = _Count++;
		Arr[idx] = item;

		return idx;
	}

	// 弹出一个元素
	const T& Pop()
	{
		assert_param(_Count > 0);

		//const T& item = Arr[--_Count];

		return Arr[--_Count];
	}

    // 重载索引运算符[]，让它可以像数组一样使用下标索引。为了性能，内部不检查下标越界，外部好自为之
    T operator[](int i)
	{
		assert_param(i >= 0 && i < _Count);
		return Arr[i];
	}
	// 列表转为指针，注意安全
    //T* operator=(Array arr) { return arr.Arr; }
};

// 变长列表模版
template<typename T>
__packed class List
{
private:
    T* arr;		// 存储数据的数组
    uint _count;// 拥有实际元素个数
    uint _total;// 可容纳元素总数

    void ChangeSize(int newSize)
    {
        if(_total == newSize) return;

        T* arr2 = new T[newSize];
        if(arr)
        {
            // 如果新数组较小，则直接复制；如果新数组较大，则先复制，再清空余下部分
            if(newSize < _count)
			{
				// 此时有数据丢失
                memcpy(arr2, arr, newSize * sizeof(T));
				_count = newSize;
			}
            else
            {
                memcpy(arr2, arr, _count * sizeof(T));
                memset(&arr2[_count], 0, (newSize - _count) * sizeof(T));
            }
            delete[] arr;
        }
		else
            memset(arr2, 0, newSize * sizeof(T));
        arr = arr2;
		_total = newSize;
    }

    void CheckSize()
    {
        // 如果数组空间已用完，则两倍扩容
        if(_count >= _total) ChangeSize(_count > 0 ? _count * 2 : 4);
    }

public:
    List(int size = 0)
    {
        _count = 0;
        _total = size;
		arr = NULL;
		if(size)
		{
			arr = new T[size];
			memset(arr, 0, size * sizeof(T));
		}
    }

    List(T* items, uint count)
    {
        arr = new T[count];

        _count = 0;
        _total = count;
        for(int i=0; i<count; i++)
        {
            arr[_count++] = *items++;
        }
    }

    ~List() { if(arr) delete[] arr; arr = NULL; }

	// 添加单个元素
    void Add(const T& item)
    {
        // 检查大小
        CheckSize();

        arr[_count++] = item;
    }

	// 添加多个元素
    void Add(T* items, int count)
    {
        int size = _count + count;
        if(size >= _total) ChangeSize(_count > 0 ? _count * 2 : 4);

        for(int i=0; i<count; i++)
        {
            arr[_count++] = *items++;
        }
    }

	// 查找元素
	int Find(const T& item)
	{
        for(int i=0; i<_count; i++)
        {
            if(arr[i] == item) return i;
        }

		return -1;
	}

	// 删除指定位置元素
	void RemoveAt(int index)
	{
		if(_count <= 0 || index >= _count) return;

		// 复制元素
		if(index < _count - 1) memcpy(&arr[index], &arr[index + 1], (_count - index - 1) * sizeof(T));
		_count--;
	}

	// 删除指定元素
	int Remove(const T& item)
	{
		int index = Find(item);
		if(index >= 0) RemoveAt(index);

		return index;
	}

    const T* ToArray()
    {
        // 如果刚好完整则直接返回，否则重新调整空间
        if(_count != _total)
        {
            T* arr2 = new T[_count];
            memcpy(arr, arr2, _count);
            delete[] arr;
            arr = arr2;
        }
        return arr;
    }

	// 有效元素个数
    int Count() { return _count; }

	// 设置新的容量，如果容量比元素个数小，则会丢失数据
	void SetCapacity(int capacity) { ChangeSize(capacity); }

    // 重载索引运算符[]，让它可以像数组一样使用下标索引。为了性能，内部不检查下标越界，外部好自为之
    T operator[](int i) { return arr[i]; }
	// 列表转为指针，注意安全
    T* operator=(List list) { return list.ToArray(); }
};

// 双向链表
template <class T> class LinkedList;

// 双向链表节点
template <class T> class LinkedNode
{
	// 友元类。允许链表类控制本类私有成员
    friend class LinkedList<T>;

public:
    T* Next;
    T* Prev;

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
};

// 双向链表
template <class T> class LinkedList
{
    T* _first;
    T* _last;

public:
    void Initialize()
    {
        _first = Tail();
        _last  = Head();
    }

	// 计算链表节点数
    int Count()
    {
        T*  ptr;
        T*  ptrNext;
        int num = 0;

        for(ptr = FirstNode(); (ptrNext = ptr->Next()) != NULL; ptr = ptrNext)
        {
            num++;
        }

        return num;
    }

    T* FirstNode() const { return _first          ; }
    T* LastNode () const { return _last           ; }
    bool IsEmpty  () const { return _first == Tail(); }

    T* FirstValidNode() const { T* res = _first; return res->Next() ? res : NULL; }
    T* LastValidNode () const { T* res = _last ; return res->Prev() ? res : NULL; }

    T* Head() const { return (T*)((uint)&_first - offsetof(T, Next)); }
    T* Tail() const { return (T*)((uint)&_last  - offsetof(T, Prev)); }

private:

	// 在两个节点之间插入新节点
    void Insert( T* prev, T* next, T* node )
    {
        node->Next = next;
        node->Prev = prev;

        next->Prev = node;
        prev->Next = node;
    }

public:
#if defined(_DEBUG)
    BOOL Exists( T* searchNode )
    {
        T* node = FirstValidNode();
        while( node != NULL && node != searchNode )
        {
            if (node == node->Next())
            {
                ASSERT(FALSE);
            }
            node = node->Next();
        }
        return (node == NULL? FALSE: TRUE);
    }
#endif

    void InsertBeforeNode( T* node, T* nodeNew )
    {
        if(node && nodeNew && node != nodeNew)
        {
            nodeNew->RemoveFromList();

            Insert( node->Prev(), node, nodeNew );
        }
    }

    void InsertAfterNode( T* node, T* nodeNew )
    {
        if(node && nodeNew && node != nodeNew)
        {
            nodeNew->RemoveFromList();

            Insert( node, node->Next(), nodeNew );
        }
    }

    void LinkAtFront( T* node )
    {
        InsertAfterNode( Head(), node );
    }

    void LinkAtBack( T* node )
    {
        InsertBeforeNode( Tail(), node );
    }

	// 释放第一个有效节点
    T* ExtractFirstNode()
    {
        T* node = FirstValidNode();

        if(node) node->Unlink();

        return node;
    }

	// 释放最后一个有效节点
    T* ExtractLastNode()
    {
        T* node = LastValidNode();

        if(node) node->Unlink();

        return node;
    }
};

#endif //_List_H_
