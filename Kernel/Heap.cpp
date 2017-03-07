#include "Kernel\Sys.h"
#include "Interrupt.h"

#include "Heap.h"
#include "TTime.h"

#define MEMORY_ALIGN	4

// 当前堆
Heap* Heap::Current = nullptr;

/*
堆分配原理：
1，使用内存块，记录当前块已使用大小以及下一块地址，形成内存块链表
2，每个内存块剩余空间是下一块指针减去当前块指针再减去已用大小
3，初始化第一个内存块，已使用0
4，申请内存时，从第一块开始找空余空间大于等于目标大小的内存块，割下来作为链表新节点
5，释放内存时，找到所在块，然后当前块上一块的Next指针直接指向当前块的下一块地址，移除当前节点
*/

/******************************** MemoryBlock ********************************/
// 内存块。大小和下一块地址
typedef struct MemoryBlock_
{
	int	Used;
	struct MemoryBlock_*	Next;
} MemoryBlock;

/******************************** Heap ********************************/
Heap::Heap(uint addr, int size)
{
	Current = this;

	// 地址对齐
	uint end = addr + size;
	addr = (addr + MEMORY_ALIGN - 1) & (~(MEMORY_ALIGN - 1));
	end = end & (~(MEMORY_ALIGN - 1));

	Address = addr;
	Size = end - addr;

	// 第一块内存块
	auto mb = (MemoryBlock*)addr;
	mb->Used = sizeof(MemoryBlock);
	mb->Next = (MemoryBlock*)(end - sizeof(MemoryBlock));
	mb->Next->Used = sizeof(MemoryBlock);
	mb->Next->Next = nullptr;

	_Used = sizeof(MemoryBlock) << 1;
	_Count = 0;

	// 记录第一个有空闲内存的块，减少内存分配时的查找次数
	_First = mb;

	debug_printf("Heap::Init(0x%p, %d) Free=%d \r\n", Address, Size, FreeSize());
}

int Heap::Used()	const { return _Used; }
int Heap::Count()	const { return _Count; }
int Heap::FreeSize()	const { return Size - _Used; }

void* Heap::Alloc(int size)
{
	// 要申请的内存大小需要对齐
	size = (size + MEMORY_ALIGN - 1) & (~(MEMORY_ALIGN - 1));

	//debug_printf("Address=%p Size=%d ", Address, Size);
	int remain = Size - _Used;
	if (size > remain)
	{
		debug_printf("Heap::Alloc %d > %d (0x%p) 失败！Size=%d Used=%d First=%p \r\n", size, remain, remain, Size, _Used, _First);
		return nullptr;
	}

#if DEBUG
	// 检查头部完整性
	auto head = (MemoryBlock*)Address;
	assert(head->Used <= Size && (byte*)head + head->Used <= (byte*)head->Next, "堆头被破坏！");
	assert(_Used <= Size, "Heap::Used异常！");
#endif

	void* ret = nullptr;
	int need = size + sizeof(MemoryBlock);

	SmartIRQ irq;
	for (auto mcb = (MemoryBlock*)_First; mcb->Next != nullptr; mcb = mcb->Next)
	{
		// 找到一块满足大小的内存块。计算当前块剩余长度
		int free = (byte*)mcb->Next - (byte*)mcb - mcb->Used;
		if (free >= need)
		{
			// 割一块出来
			auto tmp = (MemoryBlock*)((byte*)mcb + mcb->Used);
			tmp->Next = mcb->Next;
			tmp->Used = need;
			mcb->Next = tmp;

			// 记录第一个有空闲内存的块，减少内存分配时的查找次数
			_First = tmp;

			ret = (byte*)(tmp + 1);

			_Used += need;
			_Count++;

			//debug_printf("Heap::Alloc (%p, %d) First=%p Used=%d Count=%d \r\n", ret, need, _First, _Used, _Count);

			return ret;
		}
	}

	if (!ret) debug_printf("Heap::Alloc %d 失败！Count=%d Used=%d Free=%d First=%p \r\n", size, _Count, _Used, FreeSize(), _First);

	return ret;
}

void Heap::Free(void* ptr)
{
	auto prev = (MemoryBlock*)Address;
	auto cur = (MemoryBlock*)ptr - 1;

	SmartIRQ irq;
	for (auto mcb = prev->Next; mcb->Next != nullptr; mcb = mcb->Next)
	{
		// 找到内存块
		if (mcb == cur)
		{
			_Used -= cur->Used;
			_Count--;

			// 前面有空闲位置
			if (cur <= _First) _First = prev;

			//debug_printf("Heap::Free  (%p, %d) First=%p Used=%d Count=%d \r\n", ptr, cur->Used, _First, _Used, _Count);

			prev->Next = cur->Next;

			return;
		}
		prev = mcb;
	}
	debug_printf("正在释放不是本系统申请的内存 0x%p \r\n", ptr);
}
