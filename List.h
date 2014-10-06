#ifndef _List_H_
#define _List_H_

#include <stddef.h>
#include "Sys.h"

// 数组长度
//#define ArrayLength(arr) sizeof(arr)/sizeof(arr[0])
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

		return Arr[--_Count];
	}

    // 重载索引运算符[]，让它可以像数组一样使用下标索引。
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
    T* _Arr;		// 存储数据的数组
    uint _Count;// 拥有实际元素个数
    uint _total;// 可容纳元素总数

    void ChangeSize(int newSize)
    {
        if(_total == newSize) return;

        T* arr2 = new T[newSize];
        if(_Arr)
        {
            // 如果新数组较小，则直接复制；如果新数组较大，则先复制，再清空余下部分
            if(newSize < _Count)
			{
				// 此时有数据丢失
                memcpy(arr2, _Arr, newSize * sizeof(T));
				_Count = newSize;
			}
            else
            {
                memcpy(arr2, _Arr, _Count * sizeof(T));
                memset(&arr2[_Count], 0, (newSize - _Count) * sizeof(T));
            }
            delete[] _Arr;
        }
		else
            memset(arr2, 0, newSize * sizeof(T));
        _Arr = arr2;
		_total = newSize;
    }

    void CheckSize()
    {
        // 如果数组空间已用完，则两倍扩容
        if(_Count >= _total) ChangeSize(_Count > 0 ? _Count * 2 : 4);
    }

public:
    List(int size = 0)
    {
        _Count = 0;
        _total = size;
		_Arr = NULL;
		if(size)
		{
			_Arr = new T[size];
			memset(_Arr, 0, size * sizeof(T));
		}
    }

    List(T* items, uint count)
    {
        _Arr = new T[count];

        _Count = 0;
        _total = count;
        for(int i=0; i<count; i++)
        {
            _Arr[_Count++] = *items++;
        }
    }

    ~List() { if(_Arr) delete[] _Arr; _Arr = NULL; }

	// 添加单个元素
    void Add(const T& item)
    {
        // 检查大小
        CheckSize();

        _Arr[_Count++] = item;
    }

	// 添加多个元素
    void Add(T* items, int count)
    {
        int size = _Count + count;
        if(size >= _total) ChangeSize(_Count > 0 ? _Count * 2 : 4);

        for(int i=0; i<count; i++)
        {
            _Arr[_Count++] = *items++;
        }
    }

	// 查找元素
	int Find(const T& item)
	{
        for(int i=0; i<_Count; i++)
        {
            if(_Arr[i] == item) return i;
        }

		return -1;
	}

	// 删除指定位置元素
	void RemoveAt(int index)
	{
		if(_Count <= 0 || index >= _Count) return;

		// 复制元素
		if(index < _Count - 1) memcpy(&_Arr[index], &_Arr[index + 1], (_Count - index - 1) * sizeof(T));
		_Count--;
	}

	// 删除指定元素
	int Remove(const T& item)
	{
		int index = Find(item);
		if(index >= 0) RemoveAt(index);

		return index;
	}

	// 返回内部指针
    const T* ToArray() { return _Arr; }

	// 有效元素个数
    int Count() { return _Count; }

	// 设置新的容量，如果容量比元素个数小，则会丢失数据
	void SetCapacity(int capacity) { ChangeSize(capacity); }

    // 重载索引运算符[]，让它可以像数组一样使用下标索引。
    T operator[](int i)
	{
		assert_param(i >= 0 && i < _Count);
		return _Arr[i];
	}

	// 列表转为指针，注意安全
    T* operator=(List& list) { return list.ToArray(); }
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
	
	// 附加到当前节点后面
	void Append(T* node)
	{
		Next = node;
		node->Prev = (T*)this;
		//node->Next = NULL;
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
