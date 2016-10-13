#ifndef __DataStore_H__
#define __DataStore_H__

class IDataPort;

// 数据存储适配器
class DataStore
{
public:
	ByteArray	Data;	// 数据
	bool		Strict;	// 是否严格限制存储区，读写不许越界。默认true
	uint		VirAddrBase = 0;	// 虚拟地址起始位置， 可以吧Store定义到任意位置

	// 初始化
	DataStore();

	// 写入数据 offset 为虚拟地址
	int Write(uint offset, const Buffer& bs);
	// 读取数据 offset 为虚拟地址
	int Read(uint offset, Buffer& bs);

	typedef bool (*Handler)(uint offset, uint size, bool write);
	// 注册某一块区域的读写钩子函数
	void Register(uint offset, uint size, Handler hook);
	void Register(uint offset, IDataPort& port);

private:
	IList	Areas;

	bool OnHook(uint offset, uint size, bool write);
};

/****************************** 数据操作接口 ************************************/

// 数据操作接口。提供字节数据的读写能力
class IDataPort
{
public:
	virtual int Size() const { return 1; };
	virtual int Write(byte* data) { return Size(); };
	virtual int Read(byte* data) { return Size(); };

	int Write(int data) { return Write((byte*)&data); }
};

/****************************** 字节数据操作接口 ************************************/

// 字节数据操作接口
class ByteDataPort : public IDataPort
{
public:
	bool Busy;	// 是否忙于处理异步动作

	ByteDataPort();
	virtual ~ByteDataPort();

	virtual int Write(byte* data);
	virtual int Read(byte* data) { *data = OnRead(); return Size(); };

	// 让父类的所有Write函数在这里可见
	using IDataPort::Write;

	void Flush(int second);
	void FlushMs(int ms);
	void DelayOpen(int second);
	void DelayClose(int second);

protected:
	byte	Next;	// 开关延迟后的下一个状态
	uint	_tid;
	void StartAsync(int ms);
	static void AsyncTask(void* param);

	virtual int OnWrite(byte data) { return OnRead(); };
	virtual byte OnRead() { return Size(); };
};

#include "Port.h"

// 数据输出口
class DataOutputPort : public ByteDataPort, public Object
{
public:
	OutputPort*	Port;

	DataOutputPort(OutputPort* port = nullptr) { Port = port; }

protected:
	virtual int OnWrite(byte data);
	virtual byte OnRead();

	virtual String& ToStr(String& str) const;
};

// 数据输入口
class DataInputPort : public IDataPort, public Object
{
public:
	InputPort*	Port;

	DataInputPort(InputPort* port = nullptr) { Port = port; }

	virtual int Write(byte* data);
	virtual int Read(byte* data);

	virtual String& ToStr(String& str) const;
};

#endif
