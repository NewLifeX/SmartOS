#ifndef _Queue_H_
#define _Queue_H_

#include "Array.h"

// 队列
// 内有一个缓冲区，游标位置，数据长度。实际有效数据仅占用缓冲区中间部分，头尾都可能有剩余
class Queue
{
private:
	Array _s;	// 数据流
	uint _head;		// 头部位置
    uint _tail;		// 尾部位置
	uint _size;		// 长度

public:
	Queue();

	bool Empty() const { return _size == 0; }	// 队列空
	uint Capacity() const { return _s.Capacity(); }	// 队列容量
	uint Length() const { return _size; }		// 队列大小
	void SetCapacity(uint len);

	void Clear();

	void Enqueue(byte dat);
	byte Dequeue();

	uint Write(const Buffer& bs);	// 批量写入
	uint Read(Buffer& bs);		// 批量读取
};

#endif
