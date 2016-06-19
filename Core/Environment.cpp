#include <stdlib.h>
#include <time.h>

#include "Buffer.h"

#include "Environment.h"

TEnvironment Environment;

/************************************************ TEnvironment ************************************************/
TEnvironment::TEnvironment()
{
	srand(time(NULL));
}

// 程序执行起（一般为程序的开头），处理器时钟所使用的时间
UInt64 TEnvironment::Ticks() const
{
	return clock();
}

// 每秒的处理器时钟个数
uint TEnvironment::ClocksPerSecond() const
{
	return CLOCKS_PER_SEC;
}
