#ifndef __Heap_H__
#define __Heap_H__

// 堆管理
class Heap
{
public:
	uint	Address;// 开始地址
	uint	Size;	// 大小

	Heap(uint addr, uint size);
	
	uint Used() const;
	
	void* Alloc(uint size);
	void Free(void* ptr);
	
private:
};

#endif
