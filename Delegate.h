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

#include "Sys.h"

// 没有参数和返回值的委托
typedef void (*Func)(void);

// 数据接收委托
typedef void (*DataHandler)(void* sender, byte* buf, uint size);

template<typename T, typename TArg>
class Delegate
{
public:
	// 构造函数后面的冒号起分割作用，是类给成员变量赋值的方法
	// 初始化列表，更适用于成员变量的常量const型
    Delegate(T* target, void(T::*func)(TArg)) : _Target(target), _Func(func) {}

    void Invoke(TArg value)
    {
        (_Target->*_Func)(value);
    }

private:
    T* _Target;
    void (T::*_Func)(TArg);
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

#endif //_Delegate_H_
