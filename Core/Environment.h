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
};

// 全局环境变量
extern TEnvironment Environment;

#endif
