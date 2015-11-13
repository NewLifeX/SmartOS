#include "Queue.h"

Queue::Queue(uint len) : _s(len)
{
	Clear();
}

void Queue::SetCapacity(uint len)
{
	_s.SetLength(len);
}

void Queue::Clear()
{
	_s.SetLength(_s.Capacity());
	_head	= 0;
	_tail	= 0;
	_size	= 0;
}

#pragma arm section code = "SectionForSys"

void Queue::Push(byte dat)
{
	_s[_head++] = dat;
	//_head %= _s.Capacity();
	// 除法运算是一个超级大祸害，它浪费了大量时间，导致串口中断接收丢数据
	if(_head >= _s.Capacity()) _head -= _s.Capacity();

	SmartIRQ irq;
	_size++;
}

byte Queue::Pop()
{
	if(_size == 0) return 0;
	{
		SmartIRQ irq;
		_size--;
	}

	/*
	昨晚发现串口频繁收发一段数据后出现丢数据现象，也即是size为0，然后tail比head小，刚开始小一个字节，然后会逐步拉大。
	经过分析得知，ARM指令没有递加递减指令，更没有原子操作。
	size拿出来减一，然后再保存回去，但是保存回去之前，串口接收中断写入，拿到了旧的size，导致最后的size比原计划要小1。
	该问题只能通过关闭中断来解决。为了减少关中断时间以提升性能，增加了专门的Read方法。
	*/

	byte dat	= _s[_tail++];
	//_tail		%= _s.Capacity();
	if(_tail >= _s.Capacity()) _tail -= _s.Capacity();

	return dat;
}

#pragma arm section code

uint Queue::Write(const Array& bs)
{
	/*
	1，数据写入队列末尾
	2，如果还剩有数据，则从开头开始写入
	3，循环处理2
	4，如果队列过小，很有可能后来数据会覆盖前面数据
	*/

	byte*	buf	= (byte*)bs.GetBuffer();
	uint	len	= bs.Length();

	uint rs = 0;
	while(true)
	{
		// 计算这一个循环剩下的位置
		uint remain = _s.Capacity() - _head;
		// 如果要写入的数据足够存放
		if(len <= remain)
		{
			_s.Copy(buf, len, _head);
			rs		+= len;
			_head	+= len;
			if(_head >= _s.Capacity()) _head -= _s.Capacity();

			break;
		}

		// 否则先写一段，指针回到开头
		_s.Copy(buf, remain, _head);
		buf		+= remain;
		len		-= remain;
		rs		+= remain;
		_head	= 0;
	}

	SmartIRQ irq;

	_size += rs;

	return rs;
}

uint Queue::Read(Array& bs)
{
	if(_size == 0) return 0;

	//debug_printf("_head=%d _tail=%d _size=%d \r\n", _head, _tail, _size);
	//Sys.Sleep(0);

	//SmartIRQ irq;

	/*
	1，读取当前数据到末尾
	2，如果还剩有数据，则从头开始读取
	3，循环处理2
	4，如果队列过小，很有可能后来数据会覆盖前面数据
	*/

	byte*	buf	= (byte*)bs.GetBuffer();
	uint	len	= bs.Capacity();

	if(len > _size) len = _size;

	uint rs = 0;
	while(true)
	{
		// 计算这一个循环剩下的位置
		uint remain = _s.Capacity() - _tail;
		// 如果要读取的数据都在这里
		if(len <= remain)
		{
			_s.CopyTo(buf, len, _tail);
			rs		+= len;
			_tail	+= len;
			if(_tail >= _s.Capacity()) _tail -= _s.Capacity();

			break;
		}

		// 否则先读一段，指针回到开头
		_s.CopyTo(buf, remain, _tail);
		buf		+= remain;
		len		-= remain;
		rs		+= remain;
		_tail	= 0;
	}

	bs.SetLength(rs, false);

	SmartIRQ irq;
	_size -= rs;
	//if(_size == 0) _tail = _head;

	return rs;
}
