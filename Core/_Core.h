#ifndef __Core_H__
#define __Core_H__

#include <stdio.h>

extern "C"
{
#if defined(DEBUG) || defined(MSGDEBUG)

#define debug_printf printf

#else

#define debug_printf(format, ...)

#endif
}

#ifdef USE_FULL_ASSERT

// 验证确保对象不为空，并且在有效的内存范围内
//extern void assert_failed(uint8_t* file, uint32_t line);

#define assert_ptr(expr) (assert_ptr_(expr) ? (void)0 : assert_failed2("ptr==nullptr", (const char*)__FILE__, __LINE__))
bool assert_ptr_(const void* p);

void assert_failed2(const char* msg, const char* file, unsigned int line);
#define assert(expr, msg) ((expr) ? (void)0 : assert_failed2(msg, (const char*)__FILE__, __LINE__))

#else

#define assert_ptr(expr) ((void)0)
#define assert(expr, msg) ((void)0)

#endif

#endif
