#ifndef __Heap_H__
#define __Heap_H__

// 堆管理
class Heap
{
public:
	uint	Address;// 开始地址
	uint	Size;	// 大小

	Heap(uint addr, uint size);

	uint Used() const;	// 已使用内存数
	uint Count() const;	// 已使用内存块数
	uint FreeSize() const;	// 可用内存数

	void* Alloc(uint size);
	void Free(void* ptr);

	// 当前堆
	static Heap* Current;

private:
	uint	_Used;
	uint	_Count;
	void*	_First;	// 第一个有空闲的内存块，加速搜索
};

#endif
