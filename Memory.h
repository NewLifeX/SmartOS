#ifndef __Memory_H__
#define __Memory_H__

#include "Sys.h"

//#ifdef DEBUG

void* operator new(uint size);
void* operator new[](uint size);
void operator delete(void* p);
void operator delete [] (void* p);

//#define DEBUG_NEW new(__FILE__, __LINE__)
//#define new DEBUG_NEW
//#endif

// 自动释放的智能指针
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
			_ptr = NULL;
		}
	}

	void* ToPtr() { return _ptr->Ptr; }
};

// 经典的C++自动指针
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
		/*if(_ptr)
		{
			delete _ptr;
			_ptr = NULL;
		}*/
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
};

#endif
