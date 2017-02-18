#ifndef __Core_H__
#define __Core_H__

#include <stdio.h>

extern "C"
{
#if defined(DEBUG)
	#define debug_printf SmartOS_printf
	extern int SmartOS_printf(const char *format, ...);
#else
	#define debug_printf(format, ...)
#endif
}

#if defined(DEBUG)

void assert_failed2(const char* msg, const char* file, unsigned int line);
#define assert(expr, msg) ((expr) ? (void)0 : assert_failed2(msg, (const char*)__FILE__, __LINE__))

#else

#define assert(expr, msg) ((void)0)

#endif

// 关键性代码放到开头
#if !defined(TINY)
	#define INROOT __attribute__((section(".InRoot")))
#else
	#define INROOT
#endif

#endif
