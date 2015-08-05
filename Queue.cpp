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

	byte dat = _Buffer[_tail++];
	_tail %= _Capacity;
	_size--;

	return dat;
}

byte Queue::Peek() const
{
	return _Buffer[_tail];
}
