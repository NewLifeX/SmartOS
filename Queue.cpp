#include "Queue.h"

Queue::Queue(uint len)
{
	_Buffer = NULL;
	SetCapacity(len);
}

// 使用缓冲区初始化缓冲区。注意，此时指针位于0，而内容长度为缓冲区长度
Queue::Queue(ByteArray& bs)
{
	_Buffer		= bs.GetBuffer();
	_Capacity	= bs.Length();
	_size		= 0;
	_head		= 0;
	_tail		= 0;
	//Safe		= false;
}

// 销毁缓冲区
Queue::~Queue()
{
	assert_ptr(this);
	if(_needFree)
	{
		if(_Buffer != _Arr) delete[] _Buffer;
		_Buffer = NULL;
	}
}

void Queue::SetCapacity(uint len)
{
	if(len <= ArrayLength(_Arr))
	{
		// 释放旧的
		if(_needFree && _Buffer && _Buffer != _Arr) delete[] _Buffer;

		len = ArrayLength(_Arr);
		_Buffer = _Arr;
		_needFree = false;
	}
	else
	{
		_Buffer = new byte[len];
		_needFree = true;
	}

	_Capacity	= len;
	_size		= 0;
	_head		= 0;
	_tail		= 0;
}

void Queue::SetLength(uint len)
{
	_size = len;
}

void Queue::Clear()
{
	_size		= 0;
	_head		= 0;
	_tail		= 0;
}

void Queue::Push(byte dat)
{
	_Buffer[_head++] = dat;
	_head %= _Capacity;
	_size++;
}

byte Queue::Pop()
{
	if(_size == 0) return 0;
	_size--;

	/*
	昨晚发现串口频繁收发一段数据后出现丢数据现象，也即是size为0，然后tail比head小，刚开始小一个字节，然后会逐步拉大。
	经过分析得知，ARM指令没有递加递减指令，更没有原子操作。
	size拿出来减一，然后再保存回去，但是保存回去之前，串口接收中断写入，拿到了旧的size，导致最后的size比原计划要小1。
	该问题只能通过关闭中断来解决。为了减少关中断时间以提升性能，增加了专门的Read方法。
	*/

	byte dat = _Buffer[_tail++];
	_tail %= _Capacity;

	return dat;
}

byte Queue::Peek() const
{
	return _Buffer[_tail];
}

uint Queue::Write(const ByteArray& bs, bool safe)
{
	byte* buf	= bs.GetBuffer();
	uint len	= bs.Length();

	uint rs = 0;
	while(true)
	{
		// 计算这一个循环剩下的位置
		uint count = _Capacity - _head;
		// 如果要写入的数据足够存放
		if(len <= count)
		{
			memcpy(_Buffer + _head, buf, len);
			rs += len;
			_head += len;
			_head %= _Capacity;

			break;
		}

		// 否则先写一段，指针回到开头
		memcpy(_Buffer + _head, buf, count);
		buf += count;
		rs += count;
		len -= count;
		_head = 0;
	}

	if(safe)
	{
		SmartIRQ irq;
		_size += rs;
	}
	else
	{
		_size += rs;
	}

	return rs;
}

uint Queue::Read(ByteArray& bs, bool safe)
{
	if(_size == 0) return 0;

	byte* buf	= bs.GetBuffer();
	uint len	= bs.Length();

	if(len > _size) len = _size;

	uint rs = 0;
	while(true)
	{
		// 计算这一个循环剩下的位置
		uint count = _Capacity - _tail;
		// 如果要写入的数据足够存放
		if(len <= count)
		{
			memcpy(buf, _Buffer + _tail, len);
			rs += len;
			_tail += len;
			_tail %= _Capacity;

			break;
		}

		// 否则先写一段，指针回到开头
		memcpy(buf + _tail, _Buffer, count);
		buf += count;
		rs += count;
		len -= count;
		_tail = 0;
	}

	if(safe)
	{
		SmartIRQ irq;
		_size -= rs;
		if(_size == 0) _tail = _head;
	}
	else
	{
		_size -= rs;
		if(_size == 0) _tail = _head;
	}

	bs.SetLength(rs, true);

	return rs;
}
