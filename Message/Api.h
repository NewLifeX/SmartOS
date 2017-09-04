#ifndef __Api_H__
#define __Api_H__

#include "Kernel\Sys.h"
#include "Message.h"

#include "Net\IPEndPoint.h"

// 远程调用委托。传入参数名值对以及结果缓冲区引用，业务失败时返回false并把错误信息放在结果缓冲区
typedef int(*ApiHandler)(void* param, const String& args, String& result);

// 接口
class TApi
{
public:
	Dictionary<cstring, ApiHandler>	Routes;	// 路由集合
	Dictionary<cstring, void*>		Params;	// 参数集合

	// 注册远程调用处理器
	void Register(cstring action, ApiHandler handler, void* param = nullptr);
	// 模版支持成员函数
	template<typename T>
	void Register(cstring action, int(T::*func)(const String&, String&), T* target)
	{
		Register(action, *(ApiHandler*)&func, target);
	}

	// 是否包含指定动作
	bool Contain(cstring action);

	// 执行接口
	int Invoke(cstring action, void* param, const String& args, String& result);
};

// 全局对象
extern TApi Api;

#endif
