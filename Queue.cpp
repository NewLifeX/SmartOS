#include "Queue.h"

Queue::Queue(uint len)
{
	if(len <= ArrayLength(_Arr))
	{
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
	//Safe		= false;
}

// 使用缓冲区初始化缓冲区。注意，此时指针位于0，而内容长度为缓冲区长度
Queue::Queue(byte* buf, uint len)
{
	assert_ptr(buf);
	//assert_param2(len > 0, "不能用0长度缓冲区来初始化缓冲区");

	_Buffer		= buf;
	_Capacity	= len;
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

	byte dat = _Buffer[_tail++];
	_tail %= _Capacity;

	return dat;
}

byte Queue::Peek() const
{
	return _Buffer[_tail];
}

uint Queue::Write(byte* buf, uint len, bool safe)
{
	uint rs = 0;
	while(true)
	{
		// 计算这一个循环剩下的位置
		uint count = _Capacity - _head;
		// 如果要写入的数据足够存放
		if(len <= count)
		{
			memcpy(_Buffer, buf, len);
			rs += len;
			_head += len;
			_head %= _Capacity;

			break;
		}

		// 否则先写一段，指针回到开头
		memcpy(_Buffer, buf, count);
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

uint Queue::Read(byte* buf, uint len, bool safe)
{
	if(_size == 0) return 0;

	if(len > _size) len = _size;

	uint rs = 0;
	while(true)
	{
		// 计算这一个循环剩下的位置
		uint count = _Capacity - _tail;
		// 如果要写入的数据足够存放
		if(len <= count)
		{
			memcpy(buf, _Buffer, len);
			rs += len;
			_tail += len;
			_tail %= _Capacity;

			break;
		}

		// 否则先写一段，指针回到开头
		memcpy(buf, _Buffer, count);
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

	return rs;
}
