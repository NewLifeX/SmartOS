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
	// 使用字节数组初始化数据流。注意，此时指针位于0，而内容长度为缓冲区长度
	Stream(ByteArray& bs);
	// 销毁数据流
	~Stream();

	// 数据流容量
	uint Capacity() const;
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
	uint Read(byte* buf, uint offset = 0, int count = -1);
	// 读取7位压缩编码整数
	uint ReadEncodeInt();
	// 读取数据到字节数组，由字节数组指定大小。不包含长度前缀
	uint Read(ByteArray& bs);

	// 把数据写入当前位置
	bool Write(byte* buf, uint offset, uint count);
	// 写入7位压缩编码整数
	uint WriteEncodeInt(uint value);
	// 写入字符串，先写入压缩编码整数表示的长度
	uint Write(string str);
	// 把字节数组的数据写入到数据流。不包含长度前缀
	bool Write(const ByteArray& bs);

	// 从数据流读取变长数据到字节数组。以压缩整数开头表示长度
	uint ReadArray(ByteArray& bs);
	ByteArray ReadArray();
	// 把字节数组作为变长数据写入到数据流。以压缩整数开头表示长度
	bool WriteArray(const ByteArray& bs);

	// 从数据流读取变长数据到字符串。以压缩整数开头表示长度
	uint ReadString(String& str);
	String ReadString();
	// 把字符串作为变长数据写入到数据流。以压缩整数开头表示长度
	bool WriteString(const String& str);

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
	bool Write(T value)
	{
		if(!CheckRemain(sizeof(T))) return false;

		byte* p = Current();

		// 检查地址对齐
		if((uint)p % sizeof(T) == 0)
			*(T*)p = value;
		else
			memcpy(p, &value, sizeof(T));

		// 移动游标
		_Position += sizeof(T);
		if(_Position > Length) Length = _Position;

		return true;
	}

	// 读取指定长度的数据并返回首字节指针，移动数据流位置
	byte* ReadBytes(int count = -1);

	// 读取一个字节，不移动游标。如果没有可用数据，则返回-1
	int Peek() const;

private:
	bool CheckRemain(uint count);
};

#endif
