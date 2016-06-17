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

// 事件处理器
class Delegate
{
public:
	void*	Method;	// 函数指针
	void*	Target;	// 参数

	Delegate();
	Delegate(const Delegate& dlg)	= delete;
	Delegate(void* func);
	Delegate(void* func, void* target);
    Delegate(Func func);
    Delegate(Action func);
    Delegate(Action2 func);
    Delegate(Action3 func);

	template<typename T>
	Delegate(void(T::*func)(), T* target)	{ Method	= (void*)&func; Target	= target; }
	template<typename T, typename TArg>
	Delegate(void(T::*func)(TArg), T* target)	{ Method	= (void*)&func; Target	= target; }
	template<typename T, typename TArg, typename TArg2>
	Delegate(void(T::*func)(TArg, TArg2), T* target)	{ Method	= (void*)&func; Target	= target; }

	Delegate& operator=(void* func);
    Delegate& operator=(Func func);
    Delegate& operator=(Action func);
    Delegate& operator=(Action2 func);
    Delegate& operator=(Action3 func);
	
	void operator()();
	void operator()(void* arg);
	void operator()(void* arg, void* arg2);
	template<typename T>
	void operator()()
	{
		if(Method)
		{
			auto obj	= (T*)Target;
			typedef void(T::*TAction)();
			auto act	= *(TAction*)Method;

			(obj->*act)();
		}
	}
	template<typename T, typename TArg>
	void operator()(TArg arg)
	{
		if(Method)
		{
			auto obj	= (T*)Target;
			typedef void(T::*TAction)(TArg);
			auto act	= *(TAction*)Method;

			(obj->*act)(arg);
		}
	}
	template<typename T, typename TArg, typename TArg2>
	void operator()(TArg arg, TArg arg2)
	{
		if(Method)
		{
			auto obj	= (T*)Target;
			typedef void(T::*TAction)(TArg, TArg2);
			auto act	= *(TAction*)Method;

			(obj->*act)(arg, arg2);
		}
	}
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

//***************************************************************************

// 对象函数模版
template <typename TArg, typename TArg2>
class function
{
public:
	void*	Method;	// 函数指针
	void*	Target;	// 参数

	template <typename TObject>
	void bind(TObject& object, void(TObject::* func)(TArg, TArg2))
	{
		Target	= &object;
		Method	= (void*)&func;
	}

	virtual void operator ()(TArg arg, TArg2 arg2) const
	{
		// 调用对象的成员函数
		typedef void(*TFunc)(void*, TArg, TArg2);
		((TFunc)Method)(Target, arg, arg2);
	}
};

#endif //_Delegate_H_
