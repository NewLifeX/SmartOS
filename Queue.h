#ifndef _Queue_H_
#define _Queue_H_

#include <stddef.h>
#include "Sys.h"
#include "Stream.h"

// 内存缓冲区
// 缓冲区内有一个缓冲区，游标位置，数据长度。实际有效数据仅占用缓冲区中间部分，头尾都可能有剩余
class Queue
{
private:
	Stream _s;		// 数据流
	uint _head;		// 头部位置
    uint _tail;		// 尾部位置

public:
	// 分配指定大小的缓冲区
	Queue(uint len = 0);

	//bool Full() const { return _size == _Capacity; }// 队列满
	bool Empty() const { return _s.Length == 0; }	// 队列空
	uint Capacity() const { return _s.Capacity(); }	// 队列容量
	uint Length() const { return _s.Length; }		// 队列大小
	void SetCapacity(uint len);

	void Clear();

	void Push(byte dat);
	byte Pop();
	//byte Peek() const;

	uint Write(const ByteArray& bs, bool safe);	// 批量写入
	uint Read(ByteArray& bs, bool safe);		// 批量读取
};

#endif
