#ifndef _Stream_H_
#define _Stream_H_

#include <stddef.h>
#include "Sys.h"

// 数据流
// 数据流内有一个缓冲区，游标位置，数据长度。实际有效数据仅占用缓冲区中间部分，头尾都可能有剩余
class Stream
{
protected:
	byte* _Buffer;	// 数据缓冲区。扩容后会重新分配缓冲区
	uint _Capacity;	// 缓冲区容量
    uint _Position;	// 游标位置

	void Init(void* buf, uint len);
	virtual bool CheckRemain(uint count);
public:
	uint Length;	// 数据长度
	bool Little;	// 默认小字节序。仅影响数据读写操作
	bool CanWrite;	// 是否可写

	// 使用缓冲区初始化数据流。注意，此时指针位于0，而内容长度为缓冲区长度
	Stream(void* buf, uint len);
	Stream(const void* buf, uint len);
	// 使用字节数组初始化数据流。注意，此时指针位于0，而内容长度为缓冲区长度
	Stream(Array& bs);
	Stream(const Array& bs);

	// 数据流容量
	uint Capacity() const;
	void SetCapacity(uint len);
	// 当前位置
	uint Position() const;
	// 设置位置
	bool SetPosition(int p);
	// 余下的有效数据流长度。0表示已经到达终点
	uint Remain() const;
	// 尝试前后移动一段距离，返回成功或者失败。如果失败，不移动游标
	bool Seek(int offset);

	// 数据流指针。注意：扩容后指针会改变！
    byte* GetBuffer() const;
	// 数据流当前位置指针。注意：扩容后指针会改变！
    byte* Current() const;

	// 从当前位置读取数据，填充到目标数组
	uint Read(void* buf, uint offset, int count);
	// 读取7位压缩编码整数
	uint ReadEncodeInt();
	// 读取数据到字节数组，由字节数组指定大小。不包含长度前缀
	uint Read(Array& bs);

	// 把数据写入当前位置
	bool Write(const void* buf, uint offset, uint count);
	// 写入7位压缩编码整数
	uint WriteEncodeInt(uint value);
	// 写入字符串，先写入压缩编码整数表示的长度
	uint Write(const char* str);
	// 把字节数组的数据写入到数据流。不包含长度前缀
	bool Write(const Array& bs);

	// 从数据流读取变长数据到字节数组。以压缩整数开头表示长度
	uint ReadArray(Array& bs);
	ByteArray ReadArray(int count);
	// 把字节数组作为变长数据写入到数据流。以压缩整数开头表示长度
	bool WriteArray(const Array& bs);

	ByteArray ReadArray();
	String ReadString();
	bool WriteString(const String& str);

	byte	ReadByte();
	ushort	ReadUInt16();
	uint	ReadUInt32();
	ulong	ReadUInt64();

	bool Write(byte value);
	bool Write(ushort value);
	bool Write(uint value);
	bool Write(ulong value);
	bool Write(sbyte value)	{ return Write((byte)value); }
	bool Write(short value)	{ return Write((ushort)value); }
	bool Write(int value)	{ return Write((uint)value); }
	bool Write(Int64 value)	{ return Write((ulong)value); }

	// 取回指定结构体指针，并移动游标位置
	template<typename T>
	T* Retrieve(bool move = true)
	{
		int p = sizeof(T) + _Position;
		// 允许移动到最后一个字节之后，也就是Length
		if(p < 0 || p > Length) return NULL;

		T* pt = (T*)Current();
		if(move && !Seek(sizeof(T))) return NULL;

		return pt;
	}

	// 读取指定长度的数据并返回首字节指针，移动数据流位置
	byte* ReadBytes(int count = -1);

	// 读取一个字节，不移动游标。如果没有可用数据，则返回-1
	int Peek() const;
};

// 内存数据流。预分配空间，自动扩容
class MemoryStream : public Stream
{
private:
	byte _Arr[0x40];	// 内部缓冲区。较小内存需要时，直接使用栈分配，提高性能。
	bool _needFree;		// 是否自动释放
	//bool _resize;		// 是否可以自动扩容

	virtual bool CheckRemain(uint count);

public:
	// 分配指定大小的数据流
	MemoryStream(uint len = 0);
	// 使用缓冲区初始化数据流，支持自动扩容
	MemoryStream(void* buf, uint len);
	// 销毁数据流
	~MemoryStream();
};

#endif
