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
	//bool _needFree;		// 是否自动释放
	// 又是头疼的对齐问题
	uint _needFree;	// 是否自动释放
    uint _Position;	// 游标位置

	//byte _Arr[64];	// 内部缓冲区。较小内存需要时，直接使用栈分配，提高性能。
public:
	uint Length;	// 数据长度

	// 分配指定大小的数据流
	MemoryStream(uint len = 0)
	{
		//assert_param(len > 0);

		/*if(len <= ArrayLength(_Arr))
		{
			len = ArrayLength(_Arr);
			_Buffer = _Arr;
			_needFree = false;
		}
		else*/
		{
			_Buffer = new byte[len];
			assert_ptr(_Buffer);
			_needFree = true;
		}

		_Capacity = len;
		_Position = 0;
		Length = 0;
	}

	// 使用缓冲区初始化数据流。注意，此时指针位于0，而内容长度为缓冲区长度
	MemoryStream(byte* buf, uint len)
	{
		assert_ptr(buf);
		assert_param(len > 0);

		_Buffer = buf;
		_Capacity = len;
		_Position = 0;
		_needFree = false;
		Length = len;
	}

	// 销毁数据流
	virtual ~MemoryStream()
	{
		if(_needFree)
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
		// 允许移动到最后一个字节之后，也就是Length
		assert_param(p <= Length);

		_Position = p;
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

		return true;
	}

	// 数据流指针
    byte* GetBuffer() { return _Buffer; }

	// 数据流当前位置指针
    byte* Current() { return &_Buffer[_Position]; }

	// 从当前位置读取数据
	uint Read(byte* buf, uint offset = 0, int count = -1)
	{
		assert_param(buf);

		if(count == 0) return 0;

		uint remain = Remain();
		if(count < 0)
			count = remain;
		else if(count > remain)
			count = remain;

		// 复制需要的数据
		memcpy(buf + offset, Current(), count);

		// 游标移动
		_Position += count;

		return count;
	}

	// 把数据写入当前位置
	void Write(byte* buf, uint offset, uint count)
	{
		assert_param(buf);

		if(!CheckCapacity(count)) return;

		memcpy(Current(), buf + offset, count);

		_Position += count;
		//Length += count;
		// 内容长度不是累加，而是根据位置而扩大
		if(_Position > Length) Length = _Position;
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

		// 检查地址对齐
		assert_param((uint)p % sizeof(T) == 0);

		return *(T*)p;
	}

	template<typename T>
	void Write(T value)
	{
		if(!CheckCapacity(sizeof(T))) return;

		byte* p = Current();

		// 检查地址对齐
		assert_param((uint)p % sizeof(T) == 0);

		*(T*)p = value;

		// 移动游标
		_Position += sizeof(T);
		if(_Position > Length) Length = _Position;
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

private:
	bool CheckCapacity(uint count)
	{
		uint remain = _Capacity - _Position;
		// 容量不够，需要扩容
		if(count > remain)
		{
			if(!_needFree)
			{
				debug_printf("数据流剩余容量%d不足%d，而外部缓冲区无法扩容！", remain, count);
				assert_param(false);
				return false;
			}

			// 原始容量成倍扩容
			uint total = _Position + count;
			uint size = _Capacity;
			while(size < total) size <<= 1;

			// 申请新的空间，并复制数据
			byte* bufNew = new byte[size];
			if(_Position > 0) memcpy(bufNew, _Buffer, _Position);

			delete[] _Buffer;

			_Buffer = bufNew;
			_Capacity = size;
		}

		return true;
	}
};

#endif
