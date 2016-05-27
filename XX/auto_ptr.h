#ifndef _auto_ptr_H_
#define _auto_ptr_H_

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
