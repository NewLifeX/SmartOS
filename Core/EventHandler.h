#ifndef __EventHandler_H__
#define __EventHandler_H__

// 没有参数和返回值的委托
typedef void (*Func)(void);
// 一个参数没有返回值的委托，一般param参数用作目标对象，调用者用静态函数包装成员函数
typedef void (*Action)(void* param);

// 事件处理器
class EventHandler
{
public:
	void*	Method;	// 函数指针
	void*	Target;	// 参数

	EventHandler();

    EventHandler& operator=(const Func& func);
	template<typename T>
    EventHandler& operator=(void(T::*func)())		{ Method	= (void*)&func; return *this; }
	template<typename T, typename TArg>
    EventHandler& operator=(void(T::*func)(TArg))	{ Method	= (void*)&func; return *this; }
	template<typename T, typename TArg, typename TArg2>
    EventHandler& operator=(void(T::*func)(TArg, TArg2))	{ Method	= (void*)&func; return *this; }

	void Add(Func func);
	template<typename T>
	void Add(void(T::*func)(), T* target)	{ Method	= (void*)&func; Target	= target; }
	template<typename T, typename TArg>
	void Add(void(T::*func)(TArg), T* target)	{ Method	= (void*)&func; Target	= target; }
	template<typename T, typename TArg, typename TArg2>
	void Add(void(T::*func)(TArg, TArg2), T* target)	{ Method	= (void*)&func; Target	= target; }

	void operator()();
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

    /*void operator+= (const Func& func)	{ Add(func); }

	template<typename T>
    void operator+= (void(T::*func)()) { Method	= (void*)&func; }

	template<typename T, typename TArg>
    void operator+= (void(T::*func)(TArg)) { Method	= (void*)&func; }

	template<typename T, typename TArg, typename TArg2>
    void operator+= (void(T::*func)(TArg, TArg2)) { Method	= (void*)&func; }*/

private:
};

#endif
