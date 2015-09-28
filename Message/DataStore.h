#ifndef __DataStore_H__
#define __DataStore_H__

#include "Sys.h"
#include "Stream.h"

class IDataPort;

// 数据存储适配器
class DataStore
{
public:
	ByteArray	Data;	// 数据
	bool		Strict;	// 是否严格限制存储区，读写不许越界。默认true

	// 初始化
	DataStore();

	// 写入数据
	int Write(uint offset, ByteArray& bs);
	// 读取数据
	int Read(uint offset, ByteArray& bs);

	typedef bool (*Handler)(uint offset, uint size, int mode);
	// 注册某一块区域的读写钩子函数
	void Register(uint offset, uint size, Handler hook);
	void Register(uint offset, IDataPort& port);

private:
	class Area
	{
	public:
		uint	Offset;
		uint	Size;

		Handler	Hook;
		IDataPort*	Port;

		Area();
		bool Contain(uint offset, uint size);
	};

	Array<Area, 0x08> Areas;

	bool OnHook(uint offset, uint size, int mode);
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
	virtual ~ByteDataPort();

	virtual int Write(byte* data); //{ return OnWrite(*data); };
	virtual int Read(byte* data) { *data = OnRead(); return Size(); };

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
class DataOutputPort : public ByteDataPort
{
public:
	OutputPort*	Port;

	DataOutputPort(OutputPort* port = false) { Port = port; }

protected:
	virtual int OnWrite(byte data) { Port->Write(data); return OnRead(); };
	virtual byte OnRead() { return Port->Read() ? 1 : 0; };
};

#endif
