#include "_Core.h"
#include "Queue.h"

extern void EnterCritical();
extern void ExitCritical();

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

#if !defined(TINY) && defined(STM32F0)
	#if defined(__CC_ARM)
		#pragma arm section code = "SectionForSys"
	#elif defined(__GNUC__)
		__attribute__((section("SectionForSys")))
	#endif
#endif

void Queue::Enqueue(byte dat)
{
	// 溢出不再接收
	int total	= _s.Capacity();
	if(_size >= total) return;

	_s[_head++] = dat;
	//_head %= _s.Capacity();
	// 除法运算是一个超级大祸害，它浪费了大量时间，导致串口中断接收丢数据
	if(_head >= total) _head -= total;

	EnterCritical();
	_size++;
	ExitCritical();
}

byte Queue::Dequeue()
{
	if(_size == 0) return 0;

	EnterCritical();
	_size--;
	ExitCritical();

	/*
	昨晚发现串口频繁收发一段数据后出现丢数据现象，也即是size为0，然后tail比head小，刚开始小一个字节，然后会逐步拉大。
	经过分析得知，ARM指令没有递加递减指令，更没有原子操作。
	size拿出来减一，然后再保存回去，但是保存回去之前，串口接收中断写入，拿到了旧的size，导致最后的size比原计划要小1。
	该问题只能通过关闭中断来解决。为了减少关中断时间以提升性能，增加了专门的Read方法。
	*/

	int total	= _s.Capacity();
	byte dat	= _s[_tail++];
	//_tail		%= _s.Capacity();
	if(_tail >= total) _tail -= total;

	return dat;
}

#if !defined(TINY) && defined(STM32F0)
	#if defined(__CC_ARM)
		#pragma arm section code
	#elif defined(__GNUC__)
		__attribute__((section("")))
	#endif
#endif
uint Queue::Write(const Buffer& bs)
{
	// 溢出不再接收
	int total	= _s.Capacity();
	if(_size >= total) return 0;

	/*
	1，数据写入队列末尾
	2，如果还剩有数据，则从开头开始写入
	3，循环处理2
	4，如果队列过小，很有可能后来数据会覆盖前面数据
	*/

	uint	len	= bs.Length();

	// 如果队列满了，不需要覆盖
	if(_size + len > total)
		len	= total - _size;

	uint rs = 0;
	while(true)
	{
		// 计算这一个循环剩下的位置
		uint remain	= _s.Capacity() - _head;
		// 如果要写入的数据足够存放
		if(len <= remain)
		{
			_s.Copy(_head, bs, rs, len);
			rs		+= len;
			_head	+= len;
			if(_head >= _s.Length()) _head -= _s.Length();

			break;
		}

		// 否则先写一段，指针回到开头
		_s.Copy(_head, bs, rs, remain);
		len		-= remain;
		rs		+= remain;
		_head	= 0;
	}

	EnterCritical();
	_size += rs;
	ExitCritical();

	return rs;
}

uint Queue::Read(Buffer& bs)
{
	if(_size == 0) return 0;

	/*
	1，读取当前数据到末尾
	2，如果还剩有数据，则从头开始读取
	3，循环处理2
	4，如果队列过小，很有可能后来数据会覆盖前面数据
	*/

	uint	len	= bs.Length();
	if(!len) return 0;

	if(len > _size) len = _size;

	uint rs = 0;
	while(true)
	{
		int total	= _s.Capacity();
		// 计算这一个循环剩下的位置
		uint remain = total - _tail;
		// 如果要读取的数据都在这里
		if(len <= remain)
		{
			bs.Copy(rs, _s, _tail, len);
			rs		+= len;
			_tail	+= len;
			if(_tail >= total) _tail -= total;

			break;
		}

		// 否则先读一段，指针回到开头
		bs.Copy(rs, _s, _tail, remain);
		len		-= remain;
		rs		+= remain;
		_tail	= 0;
	}

	bs.SetLength(rs);

	EnterCritical();
	_size -= rs;
	ExitCritical();

	return rs;
}

WEAK void EnterCritical() { }
WEAK void ExitCritical() { }
