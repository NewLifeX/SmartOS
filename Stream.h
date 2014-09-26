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
	byte* _Cur;		// 当前数据指针
	uint _Capacity;	// 缓冲区容量
	bool _free;		// 是否自动释放
    uint _Position;	// 游标位置

public:
	uint Length;	// 数据长度

	// 分配指定大小的数据流
	MemoryStream(uint len)
	{
		assert_param(len > 0);

		_Buffer = new byte[len];
		assert_ptr(_Buffer);

		_Capacity = len;
		_Cur = _Buffer;
		_Position = 0;
		_free = true;
		Length = 0;
	}

	// 使用缓冲区初始化数据流
	MemoryStream(byte* buf, uint len)
	{
		assert_ptr(buf);
		assert_param(len > 0);

		_Buffer = buf;
		_Capacity = len;
		_Cur = _Buffer;
		_Position = 0;
		_free = false;
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

	// 当前位置
	uint Position() { return _Position; }
	// 设置位置
	void SetPosition(uint p)
	{
		int offset = p - _Position;
		_Position = p;
		_Cur += offset;
	}

	// 余下的有效数据流长度。0表示已经到达终点
	uint Remain() { return Length - _Position; };

	// 尝试前后移动一段距离，返回成功或者失败。如果失败，不移动游标
	bool Seek(int offset)
	{
		if(offset == 0) return true;

		int p = offset + _Position;
		//if(p < 0 || p >= Length) return false;
		// 允许移动到最后一个字节之后，也就是Length
		if(p < 0 || p > Length) return false;

		_Position = p;
		_Cur += offset;

		return true;
	}

	// 数据流指针
    byte* GetBuffer() { return _Buffer; }

	// 数据流当前位置指针
    byte* Current() { return _Cur; }

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
		_Position = count;
		_Cur += count;

		return count;
	}

	// 把数据写入当前位置
	void Write(byte* buf, uint offset, uint count)
	{
		assert_param(buf);

		uint remain = _Capacity - _Position;
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
			uint total = _Position + count;
			uint size = _Capacity;
			while(size < total) size <<= 1;

			// 申请新的空间，并复制数据
			byte* bufNew = new byte[size];
			if(_Position > 0) memcpy(bufNew, _Buffer, _Position);

			delete[] _Buffer;

			_Buffer = bufNew;
			_Capacity = size;
			_Cur = _Buffer + _Position;
		}

		memcpy(Current(), buf + offset, count);
		Length += count;
	}

	// 取回指定结构体指针，并移动游标位置
	template<typename T>
	T* Retrieve()
	{
		T* p = (T*)Current();
		if(!Seek(sizeof(T))) return NULL;

		return p;
	}

	// 常用读写整数方法
	template<typename T>
	T Read()
	{
		byte* p = Current();
		if(!Seek(sizeof(T))) return 0;

		return *(T*)p;
	}

	template<typename T>
	void Write(T value)
	{
		byte* p = Current();
		if(!Seek(sizeof(T))) return;

		*(T*)p = value;
	}

	byte* ReadBytes(uint count)
	{
		byte* p = Current();
		if(!Seek(count)) return NULL;

		return p;
	}

	// 读取一个字节，不移动游标。如果没有可用数据，则返回-1
	int Peek()
	{
		if(!Remain()) return -1;
		return *Current();
	}
};

#endif
