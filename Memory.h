#ifndef __Memory_H__
#define __Memory_H__

#include "Sys.h"

#ifdef DEBUG

void* operator new(uint size);
void* operator new[](uint size);
void operator delete(void * p);
void operator delete [] (void * p);

//#define DEBUG_NEW new(__FILE__, __LINE__)
//#define new DEBUG_NEW
#endif

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

#endif
