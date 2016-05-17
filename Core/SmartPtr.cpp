#include "Sys.h"

#include "Type.h"
#include "SmartPtr.h"

// 内部包装。指针的多个智能指针SmartPtr对象共用该内部包装
class WrapPtr
{
public:
	void*	Ptr;	// 目标指针
	uint	Count;	// 引用计数

	WrapPtr(void* ptr) { Ptr = ptr; Count = 1; }
	~WrapPtr() { delete Ptr; }
};

// 为某个指针封装
SmartPtr::SmartPtr(void* ptr)
{
	assert_ptr(ptr);

	_ptr = new WrapPtr(ptr);
}

// 拷贝智能指针。仅拷贝内部包装，然后引用计数加一
SmartPtr::SmartPtr(const SmartPtr& ptr)
{
	assert_ptr(ptr._ptr);
	
	auto p	= (WrapPtr*)ptr._ptr;
	assert_ptr(p->Ptr);

	_ptr = p;
	p->Count++;
}

SmartPtr::~SmartPtr()
{
	auto p	= (WrapPtr*)_ptr;
	// 释放智能指针时，指针引用计数减一
	p->Count--;
	// 如果指针引用计数为0，则释放指针
	if(!p->Count)
	{
		delete p;
		_ptr = nullptr;
	}
}

void* SmartPtr::ToPtr() { return ((WrapPtr*)_ptr)->Ptr; }
