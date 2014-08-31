#ifndef _List_H_
#define _List_H_

#include "Sys.h"

// 数组长度
#define ArrayLength(arr) sizeof(arr)/sizeof(arr[0])
// 从数组创建列表
#define MakeList(T, arr) List<T>(&arr[0], sizeof(arr)/sizeof(arr[0]))

// 定长数组模版
template<typename T, int array_size>
struct array
{
    T Arr[array_size];

    int Count() { return array_size; }

    //List<T> operator=(array arr) { return List<T>(arr.Arr, array_size); }
};

// 变长列表模版
template<typename T>
class List
{
public:
    //List() { _count = _total = 0; }
    List(int size = 0)
    {
        _count = 0;
        _total = size;
        arr = !size ? NULL : new T[size];
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
        if(size >= _total) ChangeSize(size * 2);

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
		if(index < _count - 1) memcpy(&arr[index + 1], &arr[index], (_count - index - 1) * sizeof(T));
	}

	// 删除指定元素
	int Remove(const T& item)
	{
		int index = Find(item);
		if(index >= 0) RemoveAt(index);

		return index;
	}

    T* ToArray()
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
    int Count() { return _count; }

    // 重载索引运算符[]，让它可以像数组一样使用下标索引。内部不检查下标越界，外部好自为之
    T operator[](int i) { return arr[i]; }
    T* operator=(List list) { return list.ToArray(); }

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
            if(newSize < _total)
                memcpy(arr, arr2, newSize * sizeof(T));
            else
            {
                memcpy(arr, arr2, _total * sizeof(T));
                memset(arr2 + _total, (newSize - _total) * sizeof(T));
            }
            delete[] arr;
        }
        arr = arr2;
    }

    void CheckSize()
    {
        // 如果数组空间已用完，则两倍扩容
        if(_count >= _total) ChangeSize(_count * 2);
    }
};
/*
// 双向链表
template <class T> class DblLinkedList;

// 双向链表节点
template <class T> class DblLinkedNode
{
    T* _nextNode;
    T* _prevNode;

	// 友元类。允许链表类控制本类私有成员
    friend class DblLinkedList<T>;

public:
    void Initialize()
    {
        _nextNode = NULL;
        _prevNode = NULL;
    }

    T* Next() const { return _nextNode; }
    T* Prev() const { return _prevNode; }

    void SetNext( T* next ) { _nextNode = next; }
    void SetPrev( T* prev ) { _prevNode = prev; }
	// 是否有下一个节点链接
    bool IsLinked() const { return _nextNode != NULL; }

    // 从链表中删除。需要修改前后节点的指针指向，但当前链表仍然指向之前的前后节点
    void RemoveFromList()
    {
        T* next = _nextNode;
        T* prev = _prevNode;

        if(prev) prev->_nextNode = next;
        if(next) next->_prevNode = prev;
    }

	// 完全脱离链表。不再指向其它节点
    void Unlink()
    {
        T* next = _nextNode;
        T* prev = _prevNode;

        if(prev) prev->_nextNode = next;
        if(next) next->_prevNode = prev;

        _nextNode = NULL;
        _prevNode = NULL;
    }
};

// 双向链表
template <class T> class DblLinkedList
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

    T* Head() const { return (T*)((uint)&_first - offsetof(T, _nextNode)); }
    T* Tail() const { return (T*)((uint)&_last  - offsetof(T, _prevNode)); }

private:

	// 在两个节点之间插入新节点
    void Insert( T* prev, T* next, T* node )
    {
        node->_nextNode = next;
        node->_prevNode = prev;

        next->_prevNode = node;
        prev->_nextNode = node;
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
*/
#endif //_List_H_
