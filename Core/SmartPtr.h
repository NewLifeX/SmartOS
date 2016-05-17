#ifndef _SmartPtr_H_
#define _SmartPtr_H_

// 自动释放的智能指针
class SmartPtr
{
private:
	void* _ptr;

public:
	// 为某个指针封装
	SmartPtr(void* ptr);
	// 拷贝智能指针。仅拷贝内部包装，然后引用计数加一
	SmartPtr(const SmartPtr& ptr);

	~SmartPtr();

	void* ToPtr();
};

#endif
