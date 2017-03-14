#ifndef __Heap_H__
#define __Heap_H__

// 堆管理
class Heap
{
public:
	uint	Address;// 开始地址
	int		Size;	// 大小

	Heap(uint addr, int size);

	int Used() const;	// 已使用内存数
	int Count() const;	// 已使用内存块数
	int FreeSize() const;	// 可用内存数

	void* Alloc(int size);
	void Free(void* ptr);

	// 当前堆
	static const Heap* Current;

private:
	int		_Used;
	int		_Count;
	void*	_First;	// 第一个有空闲的内存块，加速搜索
};

#endif
