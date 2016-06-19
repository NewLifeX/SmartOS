#ifndef __Environment_H__
#define __Environment_H__

// 环境变量
class TEnvironment
{
public:
	TEnvironment();

	// 程序执行起（一般为程序的开头），处理器时钟所使用的时间
	UInt64 Ticks() const;
	// 每秒的处理器时钟个数
	uint ClocksPerSecond() const;
	// 获取系统启动后经过的毫秒数
	UInt64 TickCount() const;

	// 获取系统启动后经过的毫秒数
	UInt64 Ms() const;
	// 获取系统基准秒数。加上启动后秒数即可得到绝对时间
	uint BaseSeconds() const;
	// 获取系统启动后经过的秒数
	uint Seconds() const;

	// 获取当前计算机上的处理器数
	int ProcessorCount() const;
};

// 全局环境变量
extern const TEnvironment Environment;

#endif
