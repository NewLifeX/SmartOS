#include "Api.h"

// 注册远程调用处理器
void TApi::Register(cstring action, ApiHandler handler, void* param) {
	Routes[action] = handler;
	Params[action] = param;
}

// 是否包含指定动作
bool TApi::Contain(cstring action) {
	return Routes.ContainKey(action);
}

// 执行接口
int TApi::Invoke(cstring action, void* param, const String& args, String& result) {
	ApiHandler handler;
	if (!Routes.TryGetValue(action, handler)) return -1;

	return handler(param, args, result);
}
