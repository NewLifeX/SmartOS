#ifndef _Stream_H_
#define _Stream_H_

#include <stddef.h>
#include "Sys.h"

// 内存数据流
// 数据流内有一个缓冲区，游标位置，数据长度。实际有效数据仅占用缓冲区中间部分，头尾都可能有剩余
class Stream
{
private:
	byte* _Buffer;	// 数据缓冲区。扩容后会重新分配缓冲区
	uint _Capacity;	// 缓冲区容量
	//bool _needFree;		// 是否自动释放
	// 又是头疼的对齐问题
	uint _needFree;	// 是否自动释放
    uint _Position;	// 游标位置

	byte _Arr[64];	// 内部缓冲区。较小内存需要时，直接使用栈分配，提高性能。
public:
	uint Length;	// 数据长度

	// 分配指定大小的数据流
	Stream(uint len = 0);
	// 使用缓冲区初始化数据流。注意，此时指针位于0，而内容长度为缓冲区长度
	Stream(byte* buf, uint len);
	// 销毁数据流
	~Stream();

	// 数据流容量
	uint Capacity();
	// 当前位置
	uint Position();

	// 设置位置
	void SetPosition(uint p);
	// 余下的有效数据流长度。0表示已经到达终点
	uint Remain();
	// 尝试前后移动一段距离，返回成功或者失败。如果失败，不移动游标
	bool Seek(int offset);

	// 数据流指针。注意：扩容后指针会改变！
    byte* GetBuffer();
	// 数据流当前位置指针。注意：扩容后指针会改变！
    byte* Current();

	// 从当前位置读取数据
	uint Read(byte* buf, uint offset = 0, int count = -1);
	uint ReadEncodeInt();

	// 把数据写入当前位置
	void Write(byte* buf, uint offset, uint count);
	uint WriteEncodeInt(uint value);
	// 写入字符串，先写入压缩编码整数表示的长度
	uint Write(string str);

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
		if((uint)p % sizeof(T) == 0)
			return *(T*)p;

		T obj;
		memcpy(&obj, p, sizeof(T));

		return obj;
	}

	template<typename T>
	void Write(T value)
	{
		if(!CheckCapacity(sizeof(T))) return;

		byte* p = Current();

		// 检查地址对齐
		if((uint)p % sizeof(T) == 0)
			*(T*)p = value;
		else
			memcpy(p, &value, sizeof(T));

		// 移动游标
		_Position += sizeof(T);
		if(_Position > Length) Length = _Position;
	}

	byte* ReadBytes(int count = -1);

	// 读取一个字节，不移动游标。如果没有可用数据，则返回-1
	int Peek();

private:
	bool CheckCapacity(uint count);
};

#endif
