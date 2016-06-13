#ifndef _Delegate_H_
#define _Delegate_H_

/*
成员函数指针的操作
class A{
public:
    void Func(int){...}
};

要取得Func函数指针，void (A::*pFunc)(int)=&A::Func;
::*是一个特殊操作符，表示pFunc是一个指针，指向A的成员。获取成员函数的地址不能通过类对象来获取，必须通过类名获取，而且要加上取地址操作。
那么如果通过指针来调用该函数，成员函数都隐含了一个this参数，表示函数要操作的对象，我们只获取了函数的指针，还缺少一个对象作为this参数，
为了这个目的，我们先创建一个对象，然后通过该对象来调用成员函数指针：
A a; ( a.*pFunc)(10);
A* pa=&a;
(pa->*pFunc)(11);
第一种方式通过对象本身调用，第二种方式通过对象指针调用，效果一样。

使用类模板：
要调用一个成员函数，仅仅有成员函数指针是不够的，还需要一个对象指针，所以要用一个类将两者绑到一起。
*/

// 没有参数和返回值的委托
typedef void (*Func)(void);
// 一个参数没有返回值的委托，一般param参数用作目标对象，调用者用静态函数包装成员函数
typedef void (*Action)(void* param);
// 事件处理委托，一般sender表示事件发出者，param用作目标对象，调用者用静态函数包装成员函数
typedef void (*EventHandler)(void* sender, void* param);
// 数据接收委托，一般sender表示事件发出者，param用作目标对象，调用者用静态函数包装成员函数
// 传入数据缓冲区地址和长度，如有反馈，仍使用该缓冲区，返回数据长度
typedef uint (*DataHandler)(void* sender, byte* buf, uint size, void* param);

class Delegate
{
public:
    void*	Target;
    void*	Method;
	
	// 构造函数后面的冒号起分割作用，是类给成员变量赋值的方法
	// 初始化列表，更适用于成员变量的常量const型
	template<typename T, typename TArg>
    Delegate(T* target, void(T::*func)(TArg)) : Target(target), Method(&func) {}

	template<typename T, typename TArg>
    void Invoke(TArg value)
    {
		auto obj	= (T*)Target;
		typedef void(T::*TAction)(TArg);
		auto act	= *(TAction*)Method;

		(obj->*act)(value);
    }
};

template<typename T, typename TArg, typename TArg2>
class Delegate2
{
public:
    Delegate2(T* target, void(T::*func)(TArg, TArg2)) : _Target(target), _Func(func) {}

    void Invoke(TArg value, TArg2 value2)
    {
        (_Target->*_Func)(value, value2);
    }

private:
    T* _Target;
    void (T::*_Func)(TArg, TArg2);
};

//***************************************************************************
// 函数模版接口
template <typename TParameter>
class ifunction
{
public:
	// 函数参数的类型
	typedef TParameter parameter_type;

	// 将被重载的函数操作
	virtual void operator ()(TParameter) const = 0;
};

// 无参函数模版接口
template <>
class ifunction<void>
{
public:
	typedef void parameter_type;

	virtual void operator ()() const = 0;
};

// 对象函数模版
template <typename TObject, typename TParameter>
class function : public ifunction<TParameter>
{
public:
	typedef TObject    object_type;    // 对象类型
	typedef TParameter parameter_type; // 函数参数的类型

	function(TObject& object, void(TObject::* p_function)(TParameter))
		: p_object(&object),
		p_function(p_function)
	{
	}

	virtual void operator ()(TParameter data) const
	{
		// 调用对象的成员函数
		(p_object->*p_function)(data);
	}

private:
	TObject* p_object;                        // 对象指针
	void (TObject::* p_function)(TParameter); // 成员函数指针
};

// 对象无参函数模版
template <typename TObject>
class function<TObject, void> : public ifunction<void>
{
public:
	function(TObject& object, void(TObject::* p_function)(void))
		: p_object(&object),
		p_function(p_function)
	{
	}

	virtual void operator ()() const
	{
		(p_object->*p_function)();
	}

private:
	TObject* p_object;
	void (TObject::* p_function)();
};

// 全局函数模版
template <typename TParameter>
class function<void, TParameter> : public ifunction<TParameter>
{
public:
	function(void(*p_function)(TParameter))
		: p_function(p_function)
	{
	}

	virtual void operator ()(TParameter data) const
	{
		(*p_function)(data);
	}

private:
	void (*p_function)(TParameter);
};

template <>
class function<void, void> : public ifunction<void>
{
public:
	function(void(*p_function)(void))
		: p_function(p_function)
	{
	}

	virtual void operator ()() const
	{
		(*p_function)();
	}

private:
	void (*p_function)();
};

#endif //_Delegate_H_
