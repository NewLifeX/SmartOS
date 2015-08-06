#ifndef _Queue_H_
#define _Queue_H_

#include <stddef.h>
#include "Sys.h"

// 内存缓冲区
// 缓冲区内有一个缓冲区，游标位置，数据长度。实际有效数据仅占用缓冲区中间部分，头尾都可能有剩余
class Queue
{
private:
	byte* _Buffer;	// 数据缓冲区。扩容后会重新分配缓冲区
	uint _Capacity;	// 缓冲区容量
	uint _size;		// 大小
	uint _head;		// 头部位置
    uint _tail;		// 尾部位置

	byte _Arr[64];	// 内部缓冲区。较小内存需要时，直接使用栈分配，提高性能。

	bool _needFree;	// 是否自动释放
public:
	//bool Safe;		// 是否线程安全。默认false

	// 分配指定大小的缓冲区
	Queue(uint len = 0);
	// 使用缓冲区初始化缓冲区。注意，此时指针位于0，而内容长度为缓冲区长度
	Queue(byte* buf, uint len);
	// 销毁缓冲区
	~Queue();

	bool Full() const { return _size == _Capacity; }// 队列满
	bool Empty() const { return _size == 0; }		// 队列空
	uint Capacity() const { return _Capacity; }		// 队列容量
	uint Length() const { return _size; }			// 队列大小
	void SetLength(uint len);

	void Clear();
	
	void Push(byte dat);
	byte Pop();
	byte Peek() const;
	
	uint Write(byte* buf, uint len, bool safe = false);	// 批量写入
	uint Read(byte* buf, uint len, bool safe = false);		// 批量读取
};

#endif
