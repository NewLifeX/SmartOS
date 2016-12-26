#ifndef __Memory_H__
#define __Memory_H__

#include "Kernel\Sys.h"

// 初始化内存池
bool _init_mem(void* pool, uint size)
// 分配内存
void* _alloc_mem(void* pool, uint size)
// 释放内存
bool _free_mem(void* pool, void *mem)

#endif
