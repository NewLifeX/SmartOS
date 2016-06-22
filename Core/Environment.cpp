#include "Buffer.h"

#include "Environment.h"

#if defined(__CC_ARM)
	#include <time.h>
#else
	#include <ctime>
#endif

const TEnvironment Environment;

/************************************************ TEnvironment ************************************************/

TEnvironment::TEnvironment()
{
	
}

// 获取系统启动后经过的毫秒数
UInt64 TEnvironment::TickCount() const { return clock(); }

// 获取系统启动后经过的毫秒数
UInt64 TEnvironment::Ms() const { return clock(); }

// 获取当前计算机上的处理器数
int TEnvironment::ProcessorCount() const { return 1; }
