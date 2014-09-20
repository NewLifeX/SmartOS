#ifndef _Stream_H_
#define _Stream_H_

#include <stddef.h>
#include "Sys.h"

// 内存数据流
// 数据流内有一个缓冲区，游标位置，数据长度。实际有效数据仅占用缓冲区中间部分，头尾都可能有剩余
class MemoryStream
{
private:
	byte* _Buffer;	// 数据缓冲区
	uint _Capacity;	// 缓冲区容量
	bool _free;		// 是否自动释放

	void Init()
	{
		_Buffer = NULL;
		_Capacity = 0;
		_free = false;

		Position = 0;
		Length = 0;
	}

public:
    uint Position;	// 游标位置
	uint Length;	// 数据长度

	// 分配指定大小的数据流
	MemoryStream(uint len)
	{
		assert_param(len > 0);
		Init();

		_Buffer = new byte[len];
		_Capacity = len;

		_free = true;
	}

	// 使用缓冲区初始化数据流
	MemoryStream(byte* buf, uint len)
	{
		assert_param(buf);
		assert_param(len > 0);
		Init();

		_Buffer = buf;
		_Capacity = len;
		Length = len;
	}

	// 销毁数据流
	virtual ~MemoryStream()
	{
		if(_free)
		{
			if(_Buffer) delete _Buffer;
			_Buffer = NULL;
		}
	}

	// 数据流容量
	uint Capacity() { return _Capacity; }

	// 余下的有效数据流长度
	uint Remain() { return Length - Position; };

	// 尝试前后移动一段距离，返回成功或者失败。如果失败，不移动游标
	bool TrySeek(int offset)
	{
		int p = offset + Position;
		if(p < 0 || p >= Length) return false;

		Position = p;

		return true;
	}

	// 数据流指针
    byte* GetBuffer() { return _Buffer; }

	// 数据流当前位置指针
    byte* Current()
	{
		assert_param(Position < Length);

		return _Buffer + Position;
	}

	// 从当前位置读取数据
	uint Read(byte* buf, uint offset = 0, int count = -1)
	{
		assert_param(buf);

		if(count < 0)
			count = Remain();
		else if(count > Remain())
			count = Remain();

		// 复制需要的数据
		memcpy(buf + offset, Current(), count);

		// 游标移动
		Position += count;

		return count;
	}

	// 把数据写入当前位置
	void Write(byte* buf, uint offset, uint count)
	{
		assert_param(buf);

		uint remain = _Capacity - Position;
		// 容量不够，需要扩容
		if(count > remain)
		{
			if(!_free)
			{
				debug_printf("数据流剩余容量%d不足%d，而外部缓冲区无法扩容！", remain, count);
				assert_param(false);
				return;
			}

			// 成倍扩容
			uint total = Position + count;
			uint size = _Capacity;
			while(size < total) size <<= 1;

			// 申请新的空间，并复制数据
			byte* bufNew = new byte[size];
			if(Position > 0) memcpy(bufNew, _Buffer, Position);

			delete[] _Buffer;

			_Buffer = bufNew;
			_Capacity = size;
		}

		memcpy(Current(), buf + offset, count);
		Length += count;
	}
};

#endif
