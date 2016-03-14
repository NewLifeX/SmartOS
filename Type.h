#ifndef __Type_H__
#define __Type_H__

/*#include <stdio.h>
#include <stdlib.h>*/

/* 类型定义 */
typedef char            sbyte;
typedef unsigned char   byte;
typedef unsigned short  ushort;
typedef unsigned int    uint;
typedef unsigned long long  UInt64;
typedef long long       Int64;

#define UInt64_Max 0xFFFFFFFFFFFFFFFFull

// 逐步使用char替代byte，在返回类型中使用char*替代void*
// 因为格式化输出时，可以用%c输出char，用%s输出char*

#include <typeinfo>
using namespace ::std;

class String;
class Type;

// 根对象
class Object
{
public:
	// 输出对象的字符串表示方式
	virtual String& ToStr(String& str) const;
	// 输出对象的字符串表示方式。支持RVO优化
	virtual String ToString() const;
	// 显示对象。默认显示ToString
	virtual void Show(bool newLine = false) const;

	const Type GetType() const;
};

// 类型
class Type
{
private:
	const type_info* _info;

	friend class Object;

	Type(const type_info* ti, int size);

public:
	int		Size;	// 大小
	//String	Name;	// 名称

	const String Name() const;	// 名称
};

/*
数据区设计原则：
1，固定数据区封装 Buffer	=> Object
2，变长数据区封装 Array		=> Buffer
3，自带初始缓冲区封装 ByteArray/String/TArray<T>	=> Array
*/

// 内存数据区。包装指针和长度
// 参考C#的Byte[]，主要实现对象与指针的相互转化、赋值、拷贝、设置、截取、比较等操作。
// 内部指针指向的内存和长度，都由外部传入，内部不会自动分配。
// 所有的进出拷贝都是针对内部指针和最大长度，不会自动扩容，除非子类继承扩展SetLength。
// 拷贝的原则是尽力而为，有多少可用空间就拷贝多少长度。
class Buffer : public Object
{
public:
	// 打包一个指针和长度指定的数据区
	Buffer(void* ptr = nullptr, int len = 0);
	template<typename T, int N>
	Buffer(T (&arr)[N])
	{
		_Arr	= arr;
		_Length	= sizeof(arr);
	}
	template<typename T>
	Buffer(T obj)
	{
		_Arr	= (char*)&obj;
		_Length	= sizeof(obj);
	}
	// 拷贝构造函数。直接把指针和长度拿过来用
	Buffer(const Buffer& buf) = delete;
	// 对象mov操作，指针和长度归我，清空对方
	Buffer(Buffer&& rval);

	// 从另一个对象那里拷贝，拷贝长度为两者最小者，除非当前对象能自动扩容
	// 无法解释用法，暂时注释
	Buffer& operator = (const Buffer& rhs) = delete;
	// 从指针拷贝，使用我的长度
	Buffer& operator = (const void* ptr);
	// 对象mov操作，指针和长度归我，清空对方
	Buffer& operator = (Buffer&& rval);

	// 拿出指针供外部使用
	inline byte* GetBuffer() { return (byte*)_Arr; }
	inline const byte* GetBuffer() const { return (byte*)_Arr; }
	inline int Length() const { return _Length; }

	// 设置数组长度。只能缩小不能扩大，子类可以扩展以实现自动扩容
	virtual bool SetLength(int len, bool bak = false);

    // 重载索引运算符[]，返回指定元素的第一个字节
    byte operator[](int i) const;
	// 支持 buf[i] = 0x36 的语法
    byte& operator[](int i);

	// 拷贝数据，默认-1长度表示当前长度
	virtual int Copy(int destIndex, const void* src, int len);
	// 拷贝数据，默认-1长度表示两者最小长度
	virtual int Copy(int destIndex, const Buffer& src, int srcIndex, int len);
	// 把数据复制到目标缓冲区，默认-1长度表示当前长度
	virtual int CopyTo(int srcIndex, void* dest, int len) const;

	// 用指定字节设置初始化一个区域
	int Set(byte item, int index, int len);
	void Clear(byte item = 0);

	// 截取一个子缓冲区
	//### 这里逻辑可以考虑修改为，当len大于内部长度时，直接用内部长度而不报错，方便应用层免去比较长度的啰嗦
	Buffer Sub(int index, int len);
	const Buffer Sub(int index, int len) const;

	// 显示十六进制数据，指定分隔字符和换行长度
	String& ToHex(String& str, char sep = 0, int newLine = 0) const;
	// 显示十六进制数据，指定分隔字符和换行长度
	String ToHex(char sep = 0, int newLine = 0) const;

	// 输出对象的字符串表示方式
	virtual String& ToStr(String& str) const;

    explicit operator bool() const { return _Length > 0; }
    bool operator !() const { return _Length == 0; }
	friend bool operator == (const Buffer& bs1, const Buffer& bs2);
	friend bool operator == (const Buffer& bs1, const void* ptr);
	friend bool operator != (const Buffer& bs1, const Buffer& bs2);
	friend bool operator != (const Buffer& bs1, const void* ptr);

protected:
    char*	_Arr;		// 数据指针
	int		_Length;	// 长度

	void move(Buffer& rval);
};

// 变长数组。自动扩容
class Array : public Buffer
{
public:
	// 数组最大容量。初始化时决定，后面不允许改变
	inline int Capacity() const { return _Capacity; }

	Array(void* data, int len);
	Array(const void* data, int len);
	Array(const Buffer& rhs) = delete;
	Array(const Array& rhs) = delete;
	Array(Array&& rval);

	virtual ~Array();

	Array& operator = (const Buffer& rhs) = delete;
	Array& operator = (const void* p);
	Array& operator = (Array&& rval);

	using Buffer::Set;

	// 设置数组长度。容量足够则缩小Length，否则扩容以确保数组容量足够大避免多次分配内存
	virtual bool SetLength(int length, bool bak = false);
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

protected:
	uint	_Capacity;	// 最大个数。非字节数
	bool	_needFree;	// 是否需要释放
	bool	_canWrite;	// 是否可写
	ushort	_Size;		// 单个元素大小。字节

	// 检查容量。如果不足则扩大，并备份指定长度的数据
	bool CheckCapacity(int len, int bak);
	virtual void* Alloc(int len);
	bool Release();
	void Init();

	void move(Array& rval);
};

// 数组长度
#define ArrayLength(arr) (sizeof(arr)/sizeof(arr[0]))
// 数组清零，固定长度
//#define ArrayZero(arr) memset(arr, 0, sizeof(arr))

// 使用常量数组来定义一个指针数组
//#define CArray(arr) (Array(arr, ArrayLength(arr)))
//#define SArray(obj) (Array(&obj, sizeof(obj)))

// 使用常量数组来定义一个指针数组
//#define CBuffer(arr) (Buffer(arr, ArrayLength(arr)))
//#define SBuffer(obj) (Buffer(&obj, sizeof(obj)))

// 数组
/*
数组是指针和长度的封装。
设计本意：
1，数据处理函数无需同时传递指针和长度
2，数组下标越界检查，避免指针越界
3，数组自动扩容，避免因不知道函数内部需要多大缓存空间而带来的设计复杂度

按照指针来源可分为两大用法：
1，内部分配。随时可以扩容，对象拷贝时共用空间
2，外部指针。此时认为只是封装，不允许扩容
*/
template<typename T, int ArraySize = 0x40>
class TArray : public Array
{
protected:
	T		Arr[ArraySize];	// 内部缓冲区

	virtual void* Alloc(int len) { return new T[len]; }

public:
	// 数组长度
    virtual int Length() const { return _Length; }
	// 缓冲区
	T* GetBuffer() const { return (T*)_Arr; }

	// 初始化指定长度的数组。默认使用内部缓冲区
	TArray(int length = ArraySize) : Array(Arr, ArrayLength(Arr))
	{
		assert_param2(length <= 0x400, "禁止分配超过1k的数组");
		if(length < 0) length = ArrayLength(Arr);

		_Length		= length;
		if(length > ArrayLength(Arr))
		{
			_Arr		= (char*)new T[length];
			_Capacity	= length;
			_needFree	= true;
		}

		_Size	= sizeof(T);
	}

	// 重载等号运算符，使用另一个固定数组来初始化
    /*TArray& operator=(const TArray& arr)
	{
		Array::operator=(arr);

		return *this;
	}*/
	TArray& operator=(const TArray& arr) = delete;

	// 让父类的所有Set函数在这里可见
	using Array::Set;

	// 设置数组元素为指定值，自动扩容
	bool Set(const T& item, int index, int count)
	{
		return SetItem(&item, index, count);
	}

	// 设置指定位置的值，不足时自动扩容
	virtual void SetAt(int i, const T& item)
	{
		SetItemAt(i, &item);
	}

	// 加入一个数据到末尾
	virtual int Push(const T& item)
	{
		SetItemAt(_Length, &item);

		return _Length;
	}

	// 末尾加入一个空数据，并返回引用，允许外部修改
	virtual T& Push()
	{
		assert_param2(_canWrite, "禁止修改");

		int i = _Length;
		// 检查长度，不足时扩容
		CheckCapacity(i + 1, _Length);

		_Length++;

		T* buf = (T*)_Arr;
		return buf[i];
	}

	// 弹出最后一个数组元素，长度减一
	virtual T& Pop()
	{
		assert_param2(_canWrite, "禁止修改");

		T* buf = (T*)_Arr;
		return buf[--_Length];
	}

	// 查找元素
	virtual int FindIndex(const T item)
	{
		T* buf = (T*)_Arr;
		for(int i=0; i<Length(); i++)
		{
			if(buf[i] == item) return i;
		}

		return -1;
	}

    // 重载索引运算符[]，让它可以像数组一样使用下标索引。
    virtual T& operator[](int i) const
	{
		assert_param2(_Arr && i >= 0 && i < _Length, "下标越界");

		T* buf = (T*)_Arr;
		return buf[i];
	}
};

// 字节数组
class ByteArray : public Array
{
public:
	ByteArray(int length = 0);
	ByteArray(byte item, int length);
	// 因为使用外部指针，这里初始化时没必要分配内存造成浪费
	ByteArray(const void* data, int length, bool copy = false);
	ByteArray(void* data, int length, bool copy = false);
	ByteArray(const Buffer& arr) = delete;
	ByteArray(const ByteArray& arr) = delete;
	ByteArray(ByteArray&& rval);
	//ByteArray(String& str);			// 直接引用数据缓冲区
	//ByteArray(const String& str);	// 不允许修改，拷贝

	ByteArray& operator = (const Buffer& rhs) = delete;
	ByteArray& operator = (const ByteArray& rhs) = delete;
	ByteArray& operator = (const void* p);
	ByteArray& operator = (ByteArray&& rval);

	// 重载等号运算符，使用外部指针、内部长度，用户自己注意安全
    //ByteArray& operator=(const void* data);

	// 保存到普通字节数组，首字节为长度
	int Load(const void* data, int maxsize = -1);
	// 从普通字节数据组加载，首字节为长度
	int Save(void* data, int maxsize = -1) const;

	ushort	ToUInt16() const;
	uint	ToUInt32() const;
	UInt64	ToUInt64() const;
	void Write(ushort value, int index = 0);
	void Write(short value, int index = 0);
	void Write(uint value, int index = 0);
	void Write(int value, int index = 0);
	void Write(UInt64 value, int index = 0);

    //friend bool operator==(const ByteArray& bs1, const ByteArray& bs2);
    //friend bool operator!=(const ByteArray& bs1, const ByteArray& bs2);

protected:
	byte	Arr[0x40];	// 内部缓冲区

	virtual void* Alloc(int len) { return new byte[len]; }

	void move(ByteArray& rval);
};

// 从数组创建列表
#define MakeList(T, arr) List<T>(&arr[0], sizeof(arr)/sizeof(arr[0]))

/*// 变长列表模版
// T一般是指针，列表内部有一个数组用于存放指针
template<typename T>
class List : public TArray<T*>
{
public:
    List(int size = 0) : TArray(size) { _Length = 0; }
    List(T* items, uint count) { Set(items, count); }

	// 添加单个元素
    virtual void Add(const T& item) { Push(item); }

	// 添加多个元素
    void Add(T* items, int count)
    {
        for(int i=0; i<count; i++) Push(*items++);
    }

	// 删除指定位置元素
	void RemoveAt(int index)
	{
		int len = Length();
		if(len <= 0 || index >= len) return;

		// 复制元素
		if(index < len - 1) memmove(&_Arr[index], &_Arr[index + 1], (len - index - 1) * sizeof(T));
		_Length--;
	}

	// 删除指定元素
	virtual void Remove(const T& item)
	{
		int index = FindIndex(item);
		if(index >= 0) RemoveAt(index);
	}

	// 释放所有指针指向的内存
	List& DeleteAll()
	{
		for(int i=0; i < Length(); i++)
		{
			if(_Arr[i]) delete _Arr[i];
		}

		return *this;
	}

	virtual void Clear() { _Length = 0; }

	// 返回内部指针
    const T* ToArray() { return _Arr; }
};*/

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

/*void* operator new(uint size) throw(std::bad_alloc);
void* operator new[](uint size) throw(std::bad_alloc);
void operator delete(void* p) throw();
void operator delete[](void* p) throw();*/

//#define DEBUG_NEW new(__FILE__, __LINE__)
//#define new DEBUG_NEW
//#endif

/*// 自动释放的智能指针
class SmartPtr
{
private:
	// 内部包装。指针的多个智能指针SmartPtr对象共用该内部包装
	class WrapPtr
	{
	public:
		void*	Ptr;	// 目标指针
		uint	Count;	// 引用计数

		WrapPtr(void* ptr) { Ptr = ptr; Count = 1; }
		~WrapPtr() { delete Ptr; }
	};
	WrapPtr* _ptr;

public:
	// 为某个指针封装
	SmartPtr(void* ptr)
	{
		assert_ptr(ptr);

		_ptr = new WrapPtr(ptr);
	}

	// 拷贝智能指针。仅拷贝内部包装，然后引用计数加一
	SmartPtr(const SmartPtr& ptr)
	{
		assert_ptr(ptr._ptr);
		assert_ptr(ptr._ptr->Ptr);

		_ptr = ptr._ptr;
		_ptr->Count++;
	}

	~SmartPtr()
	{
		// 释放智能指针时，指针引用计数减一
		_ptr->Count--;
		// 如果指针引用计数为0，则释放指针
		if(!_ptr->Count)
		{
			delete _ptr;
			_ptr = nullptr;
		}
	}

	void* ToPtr() { return _ptr->Ptr; }
};*/

/*// 经典的C++自动指针
// 超出对象作用域时自动销毁被管理指针
template<class T>
class auto_ptr
{
private:
	T* _ptr;

public:
	// 普通指针构造自动指针，隐式转换
	// 构造函数的explicit关键词有效阻止从一个“裸”指针隐式转换成auto_ptr类型
	explicit auto_ptr(T* p = 0) : _ptr(p) { }
	// 拷贝构造函数，解除原来自动指针的管理权
	auto_ptr(auto_ptr& ap) : _ptr(ap.release()) { }
	// 析构时销毁被管理指针
	~auto_ptr()
	{
		// 因为C++保证删除一个空指针是安全的，所以我们没有必要判断空
		delete _ptr;
	}

	// 自动指针拷贝，解除原来自动指针的管理权
	auto_ptr& operator=(T* p)
	{
		_ptr = p;
		return *this;
	}

	// 自动指针拷贝，解除原来自动指针的管理权
	auto_ptr& operator=(auto_ptr& ap)
	{
		reset(ap.release());
		return *this;
	}

	// 获取原始指针
	T* get() const { return _ptr; }

	// 重载*和->运算符
	T& operator*() const { assert_param(_ptr); return *_ptr; }
	T* operator->() const { return _ptr; }

	// 接触指针的管理权
	T* release()
	{
		T* p = _ptr;
		_ptr = 0;
		return p;
	}

	// 销毁原指针，管理新指针
	void reset(T* p = 0)
	{
		if(_ptr != p)
		{
			//if(_ptr) delete _ptr;
			// 因为C++保证删除一个空指针是安全的，所以我们没有必要判断空
			delete _ptr;
			_ptr = p;
		}
	}
};*/

#endif
