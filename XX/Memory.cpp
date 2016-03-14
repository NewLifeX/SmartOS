#include "Memory.h"

// 内存管理结构
typedef struct mem
{
	struct mem *next;	// 下一个内存块
	uint         len;	// 数据块长度
} MemBlock;

typedef struct
{
  void *free;                     /* Pointer to first free memory block      */
  void *end;                      /* Pointer to memory block end             */
  U32  blk_size;                  /* Memory block size                       */
} Block;

// 初始化内存池
bool _init_mem(void* pool, uint size)
{
	if (pool == nullptr || size < sizeof(MemBlock)) return false;

	/*
	现有内存块位于pool，大小size。
	首先我们得在开头搞一个MEMP结构，指示这是第一个内存块。
	再者在这个内存块最后面也搞一个MEMP结构，指示这是最后一个内存块，它的Next是空。
	所以，第一个内存块的Next指向最后一个内存块。
	因为还没有用，所以大小是0，实际持有空闲大小是Next减去当前指针。
	*/
	MemBlock* ptr = (MemBlock*)pool;
	ptr->next = (MemBlock*)((uint)pool + size - sizeof(MemBlock));
	ptr->next->next = nullptr;
	ptr->len = 0;

	return true;
}

// 分配内存
void* _alloc_mem(void* pool, uint size)
{
	if (pool == nullptr || size == 0) return nullptr;

	// 增加头部大小
	size += sizeof(MemBlock);
	// 确保4字节对齐
	size = (size + 3) & ~3;

	MemBlock* p = (MemBlock*)pool;
	while (true)
	{
		// 当前块总大小，扣掉已经用掉的大小，得到可用大小
		uint hole_size  = (uint)p->next - (uint)p;
		hole_size -= p->len;
		// 检查大小是否足够
		if (hole_size >= size) break;
		p = p->next;
		// 失败，位于列表尾部
		if (p->next == nullptr) return nullptr;
	}

	if (p->len == 0)
	{
		// 还没有分配任何块，设置第一个元素的长度
		p->len = size;
		return (MemBlock*)((uint)p + sizeof(MemBlock));
	}
	else
	{
		// 插入新的元素到内存列表
		MemBlock* p_new	= (MemBlock*)((uint)p + p->len);
		p_new->next = p->next;
		p_new->len  = size;
		p->next = p_new;
		return (MemBlock*)((uint)p_new + sizeof(MemBlock));
	}
}

// 释放内存
bool _free_mem(void* pool, void *mem)
{
	if (pool == nullptr || mem == nullptr) return false;

	// 要释放的目标内存块
	MemBlock* rs	= (MemBlock*)((uint)mem - sizeof(MemBlock));

	MemBlock* prv	= nullptr;
	MemBlock* p		= (MemBlock*)pool;
	while (p != rs)
	{
		prv	= p;
		p	= p->next;
		// 验证内存块不存在
		if (p == nullptr) return false;
	}

	if (prv == nullptr)
	// 第一块被释放，仅仅设置长度为0
		p->len		= 0;
	else
	// 从链表中移除块
		prv->next	= p->next;

	return true;
}

int _init_box(void* box_mem, uint box_size, uint blk_size)
{
	/* Initialize memory block system, returns 0 if OK, 1 if fails. */
	void *end;
	void *blk;
	void *next;
	uint  sizeof_bm;

	// 建立内存结构，注意8字节对齐还是4字节对齐
	if (blk_size & BOX_ALIGN_8) {
		blk_size	= ((blk_size & ~BOX_ALIGN_8) + 7) & ~7;
		sizeof_bm	= (sizeof (struct OS_BM) + 7) & ~7;
	}
	else {
		blk_size	= (blk_size + 3) & ~3;
		sizeof_bm	= sizeof (struct OS_BM);
	}
	if (blk_size == 0) return false;
	if (blk_size + sizeof_bm > box_size) return false;
	
	// 建立内存结构
	blk = ((byte *) box_mem) + sizeof_bm;
	((Block*) box_mem)->free = blk;
	end = ((byte *) box_mem) + box_size;
	((Block*) box_mem)->end      = end;
	((Block*) box_mem)->blk_size = blk_size;

	/* Link all free blocks using offsets. */
	end = ((byte *) end) - blk_size;
	while (1)  {
		next = ((byte *) blk) + blk_size;
		if (next > end)  break;
		*((void **)blk) = next;
		blk = next;
	}
	/* end marker */
	*((void **)blk) = 0;
	return true;
}

void* _alloc_box (void *box_mem)
{
	/* Allocate a memory block and return start address. */
	void **free;
	do {
		if ((free = (void **)__ldrex(&((Block*) box_mem)->free)) == 0) {
			__clrex();
			break;
		}
	} while (__strex((uint)*free, &((Block*) box_mem)->free));

	return free;
}

void* _calloc_box(void* box_mem)
{
	/* Allocate a 0-initialized memory block and return start address. */
	void *free;
	uint *p;
	uint i;

	free = _alloc_box (box_mem);
	if (free)
	{
		p = free;
		for (i = ((Block*) box_mem)->blk_size; i; i -= 4)
		{
			*p = 0;
			p++;
		}
	}
	return (free);
}

bool _free_box(void* box_mem, void* box)
{
	if (box < box_mem || box >= ((Block*) box_mem)->end) return false;

	do {
		*((void **)box) = (void *)__ldrex(&((Block*) box_mem)->free);
	} while (__strex ((uint)box, &((Block*) box_mem)->free));
	return true;
}
