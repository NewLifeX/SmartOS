#ifndef _Delegate_H_
#define _Delegate_H_

// 没有参数和返回值的委托
typedef void (*Func)(void);
// 一个参数没有返回值的委托，一般param参数用作目标对象，调用者用静态函数包装成员函数
typedef void (*Action)(void* param);
typedef void(*Action2)(void*, void*);
typedef void(*Action3)(void*, void*, void*);
// 事件处理委托，一般sender表示事件发出者，param用作目标对象，调用者用静态函数包装成员函数
typedef void (*EventHandler)(void* sender, void* param);
// 传入数据缓冲区地址和长度，如有反馈，仍使用该缓冲区，返回数据长度
typedef uint (*DataHandler)(void* sender, byte* buf, uint size, void* param);

// 委托接口
class IDelegate
{
public:
	void*	Method;	// 函数指针
	void*	Target;	// 参数

protected:
	void Bind(void* method, void* target = nullptr)
	{
		Method	= method;
		Target	= target;
	}
};

// 委托。第一参数目标对象指针，第二泛型参数
template <typename TArg>
class Delegate : public IDelegate
{
public:
	typedef void(*TAction)(TArg);
	typedef void(*VAction)(void*, TArg);

	Delegate()	{ Bind(nullptr); }
	Delegate(const Delegate& dlg)	= delete;

	// 全局函数或类静态函数
    Delegate(Action func)	{ Bind((void*)func); }
    Delegate(TAction func)	{ Bind((void*)func); }
    Delegate& operator=(Action func)	{ Bind((void*)func); return *this; }
    Delegate& operator=(TAction func)	{ Bind((void*)func); return *this; }

	// 带目标的全局函数
	template<typename T>
	Delegate(void(*func)(T&, TArg), T* target)	{ Bind((void*)func, target); }

	// 类成员函数
	template<typename T>
	Delegate(void(T::*func)(TArg), T* target)	{ Bind((void*)&func, target); }

	// 执行委托
	void operator()(TArg arg)
	{
		if(Method)
		{
			if(Target)
				(*(VAction*)Method)(Target, arg);
			else
				(*(TAction*)Method)(arg);
		}
	}
};

// 委托。第一参数目标对象指针，第二第三泛型参数
template <typename TArg, typename TArg2>
class Delegate2 : public IDelegate
{
public:
	typedef void(*TAction)(TArg, TArg2);
	typedef void(*VAction)(void*, TArg, TArg2);

	Delegate2()	{ Bind(nullptr); }
	Delegate2(const Delegate2& dlg)	= delete;

	// 全局函数或类静态函数
    Delegate2(Action2 func)	{ Bind((void*)func); }
    Delegate2(TAction func)	{ Bind((void*)func); }
    Delegate2& operator=(Action2 func)	{ Bind((void*)func); return *this; }
    Delegate2& operator=(TAction func)	{ Bind((void*)func); return *this; }

	// 带目标的全局函数
	template<typename T>
	Delegate2(void(*func)(T&, TArg, TArg2), T* target)	{ Bind((void*)func, target); }

	// 类成员函数
	template<typename T>
	Delegate2(void(T::*func)(TArg, TArg2), T* target)	{ Bind((void*)&func, target); }

	// 执行委托
	void operator()(TArg arg, TArg2 arg2)
	{
		if(Method)
		{
			if(Target)
				(*(VAction*)Method)(Target, arg, arg2);
			else
				(*(TAction*)Method)(arg, arg2);
		}
	}
};

// 委托。第一参数目标对象指针，第二泛型参数
template <typename TArg, typename TArg2, typename TArg3>
class Delegate3 : public IDelegate
{
public:
	typedef void(*TAction)(TArg, TArg2, TArg3);
	typedef void(*VAction)(void*, TArg, TArg2, TArg3);

	Delegate3()	{ Bind(nullptr); }
	Delegate3(const Delegate3& dlg)	= delete;

	// 全局函数或类静态函数
    Delegate3(Action3 func)	{ Bind((void*)func); }
    Delegate3(TAction func)	{ Bind((void*)func); }
    Delegate3& operator=(Action3 func)	{ Bind((void*)func); return *this; }
    Delegate3& operator=(TAction func)	{ Bind((void*)func); return *this; }

	// 带目标的全局函数
	template<typename T>
	Delegate3(void(*func)(T&, TArg, TArg2, TArg3), T* target)	{ Bind((void*)func, target); }

	// 类成员函数
	template<typename T>
	Delegate3(void(T::*func)(TArg, TArg2, TArg3), T* target)		{ Bind((void*)&func, target); }

	// 执行委托
	void operator()(TArg arg, TArg2 arg2, TArg3 arg3)
	{
		if(Method)
		{
			if(Target)
				(*(VAction*)Method)(Target, arg, arg2, arg3);
			else
				(*(TAction*)Method)(arg, arg2, arg3);
		}
	}
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

#endif //_Delegate_H_
