#include "Api.h"

// 全局对象
TApi Api;

TApi::TApi() :Routes(String::Compare), Params(String::Compare) {

}

// 注册远程调用处理器
void TApi::Register(cstring action, ApiHandler handler, void* param) {
	Routes.Add(action, handler);
	Params.Add(action, param);
}

// 是否包含指定动作
bool TApi::Contain(cstring action) {
	return Routes.ContainKey(action);
}

// 执行接口
int TApi::Invoke(cstring action, const String& args, String& result) {
	ApiHandler handler;
	if (!Routes.TryGetValue(action, handler)) return -1;

	void* p = Params[action];

	return handler(p, args, result);
}
