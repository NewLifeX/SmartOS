#ifndef __Environment_H__
#define __Environment_H__

// 环境变量
class TEnvironment
{
public:
	// 获取系统启动后经过的毫秒数
	UInt64 TickCount() const;

	// 获取系统启动后经过的毫秒数
	UInt64 Ms() const;

	// 获取当前计算机上的处理器数
	int ProcessorCount() const;
};

// 全局环境变量
extern const TEnvironment Environment;

#endif
