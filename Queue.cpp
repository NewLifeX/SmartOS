#include "Queue.h"

Queue::Queue(uint len) : _s(len)
{
	_s.SetPosition(0);
	_s.Length	= 0;
	_head		= 0;
	_tail		= 0;
}

void Queue::SetCapacity(uint len)
{
	_s.SetCapacity(len);
}

void Queue::Clear()
{
	_s.Length	= 0;
	_s.SetPosition(0);
	_head	= 0;
	_tail	= 0;
}

void Queue::Push(byte dat)
{
	byte* buf = _s.GetBuffer();
	buf[_head++] = dat;
	_head++;
	_head %= _s.Capacity();
	_s.Length++;
}

byte Queue::Pop()
{
	if(_s.Length == 0) return 0;
	_s.Length--;

	/*
	昨晚发现串口频繁收发一段数据后出现丢数据现象，也即是size为0，然后tail比head小，刚开始小一个字节，然后会逐步拉大。
	经过分析得知，ARM指令没有递加递减指令，更没有原子操作。
	size拿出来减一，然后再保存回去，但是保存回去之前，串口接收中断写入，拿到了旧的size，导致最后的size比原计划要小1。
	该问题只能通过关闭中断来解决。为了减少关中断时间以提升性能，增加了专门的Read方法。
	*/

	byte* buf	= _s.GetBuffer();
	byte dat	= buf[_tail++];
	_tail		%= _s.Capacity();

	return dat;
}

/*byte Queue::Peek() const
{
	return _s.GetBuffer()[_tail];
}*/

uint Queue::Write(const ByteArray& bs, bool safe)
{
	byte* buf	= bs.GetBuffer();
	uint len	= bs.Length();

	uint rs = 0;
	while(true)
	{
		// 计算这一个循环剩下的位置
		uint count = _s.Capacity() - _head;
		// 如果要写入的数据足够存放
		if(len <= count)
		{
			memcpy(_s.GetBuffer() + _head, buf, len);
			rs		+= len;
			_head	+= len;
			_head	%= _s.Capacity();

			break;
		}

		// 否则先写一段，指针回到开头
		memcpy(_s.GetBuffer() + _head, buf, count);
		buf		+= count;
		rs		+= count;
		len		-= count;
		_head	= 0;
	}

	if(safe)
	{
		SmartIRQ irq;
		_s.Length += rs;
	}
	else
	{
		_s.Length += rs;
	}

	return rs;
}

uint Queue::Read(ByteArray& bs, bool safe)
{
	if(_s.Length == 0) return 0;

	byte* buf	= bs.GetBuffer();
	uint len	= bs.Capacity();

	if(len > _s.Length) len = _s.Length;

	uint rs = 0;
	while(true)
	{
		// 计算这一个循环剩下的位置
		uint count = _s.Capacity() - _tail;
		// 如果要写入的数据足够存放
		if(len <= count)
		{
			memcpy(buf, _s.GetBuffer() + _tail, len);
			rs += len;
			_tail += len;
			_tail %= _s.Capacity();

			break;
		}

		// 否则先写一段，指针回到开头
		memcpy(buf + _tail, _s.GetBuffer(), count);
		buf += count;
		rs += count;
		len -= count;
		_tail = 0;
	}

	if(safe)
	{
		SmartIRQ irq;
		_s.Length -= rs;
		if(_s.Length == 0) _tail = _head;
	}
	else
	{
		_s.Length -= rs;
		if(_s.Length == 0) _tail = _head;
	}

	bs.SetLength(rs, true);

	return rs;
}
