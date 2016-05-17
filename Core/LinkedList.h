#ifndef _LinkedList_H_
#define _LinkedList_H_

// 双向链表
template <class T> class LinkedList;

// 双向链表节点。实体类继承该类
template <class T> class LinkedNode
{
	// 友元类。允许链表类控制本类私有成员
    friend class LinkedList<T>;

public:
    T* Prev;	// 上一个节点
    T* Next;	// 下一个节点

    void Initialize()
    {
        Next = nullptr;
        Prev = nullptr;
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

        Next = nullptr;
        Prev = nullptr;
    }

	// 把当前节点附加到另一个节点之后
	void LinkAfter(T* node)
	{
		node->Next = (T*)this;
		Prev = node;
		// 不能清空Next，因为可能是两个链表的合并
		//Next = nullptr;
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
		//node->Next = nullptr;
	}
};

/*// 双向链表
template <class T>
class LinkedList
{
public:
    // 链表节点。实体类不需要继承该内部类
	class Node
	{
	public:
		T		Item;	// 元素
		Node*	Prev;	// 前一节点
		Node*	Next;	// 下一节点

		Node()
		{
			Prev = nullptr;
			Next = nullptr;
		}

		// 从队列中脱离
		void RemoveFromList()
		{
			// 双保险，只有在前后节点指向当前节点时，才修改它们的值
			if(Prev && Prev->Next == this) Prev->Next = Next;
			if(Next && Next->Prev == this) Next->Prev = Prev;
		}

		// 附加到当前节点后面
		void Append(Node* node)
		{
			Next = node;
			node->Prev = this;
		}
	};

private:
	Node*	_Head;		// 链表头部
    Node*	_Tail;		// 链表尾部
	int 	_Count;		// 元素个数

	void Init()
	{
        _Head = nullptr;
		_Tail = nullptr;
		_Count = 0;
	}

	void Remove(Node* node)
	{
		// 脱离队列
		node->RemoveFromList();
		// 特殊处理头尾
		if(node == _Head) _Head = node->Next;
		if(node == _Tail) _Tail = node->Prev;

		delete node;
		_Count--;
	}

public:
    LinkedList()
    {
		Init();
    }

	// 计算链表节点数
    virtual int Count() const { return _Count; }

	// 将某项添加到集合
	virtual void Add(const T& item)
	{
		Node* node = new Node();
		node->Item = item;

		if(_Tail)
			_Tail->Append(node);
		else
			_Head = _Tail = node;

		_Count++;
	}

	// 从集合中移除特定对象的第一个匹配项
	virtual void Remove(const T& item)
	{
		if(!_Count) return;

		Node* node;
		for(node = _Head; node; node = node->Next)
		{
			if(node->Item == item) break;
		}

		if(node) Remove(node);
	}

	// 确定集合是否包含特定值
	virtual bool Contains(const T& item)
	{
		if(!_Count) return false;

		Node* node;
		for(node = _Head; node; node = node->Next)
		{
			if(node->Item == item) return true;
		}

		return false;
	}

	// 从集合中移除所有项。注意，该方法不会释放元素指针
	virtual void Clear()
	{
		if(!_Count) return;

		Node* node;
		for(node = _Head; node;)
		{
			// 先备份，待会删除可能影响指针
			Node* next = node->Next;
			delete node;
			node = next;
		}

		Init();
	}

	// 将集合元素复制到数组中
	virtual void CopyTo(T* arr)
	{
		assert_ptr(arr);

		if(!_Count) return;

		Node* node;
		for(node = _Head; node; node = node->Next)
		{
			*arr++ = node->Item;
		}
	}

	T& First() { return _Head->Item; }
	T& Last() { return _Tail->Item; }

	// 释放第一个有效节点
    T& ExtractFirst()
    {
		if(!_Count) return nullptr;

        Node* node = _Head;
        _Head = _Head->Next;
		// 可能只有一个节点
		if(!_Head)
			_Tail = nullptr;
		else
			_Head->Prev = nullptr;

		T& item = node->Item;
		delete node;
		_Count--;

        return item;
    }

	// 释放最后一个有效节点
    T& ExtractLast()
    {
		if(!_Count) return nullptr;

        Node* node = _Tail;
        _Tail = _Tail->Prev;
		// 可能只有一个节点
		if(!_Tail)
			_Head = nullptr;
		else
			_Tail->Next = nullptr;

		T& item = node->Item;
		delete node;
		_Count--;

        return item;
    }
};*/

#endif
