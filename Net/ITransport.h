#ifndef __ITransport_H__
#define __ITransport_H__

#include "Sys.h"

class ITransport;

// 传输口数据到达委托。传入数据缓冲区地址和长度，如有反馈，仍使用该缓冲区，返回数据长度
typedef uint (*TransportHandler)(ITransport* port, ByteArray& bs, void* param, void* param2);

// 帧数据传输接口
// 实现者确保数据以包的形式传输，屏蔽数据的粘包和拆包
class ITransport
{
private:
	TransportHandler _handler;
	void* _param;

public:
	bool Opening;	// 是否正在打开
    bool Opened;    // 是否打开

	// 初始化
	ITransport()
	{
		Opening		= false;
		Opened		= false;
		_handler	= NULL;
		_param		= NULL;
	}

	// 析构函数确保关闭
	virtual ~ITransport()
	{
		if(Opened) Close();

		Register(NULL);
	}

	// 打开传输口
	bool Open()
	{
		// 特别是接口要检查this指针
		assert_ptr(this);

		if(Opened || Opening) return true;

		Opening	= true;
		Opened	= OnOpen();
		Opening	= false;

		return Opened;
	}

	// 关闭传输口
	void Close()
	{
		// 特别是接口要检查this指针
		assert_ptr(this);

		if(!Opened || Opening) return;

		Opening	= true;
		OnClose();
		Opening	= false;
		Opened	= false;
	}

	// 发送数据
	bool Write(const ByteArray& bs)
	{
		// 特别是接口要检查this指针
		assert_ptr(this);

		if(!Opened && !Open()) return false;

		return OnWrite(bs);
	}

	// 接收数据
	uint Read(ByteArray& bs)
	{
		// 特别是接口要检查this指针
		assert_ptr(this);

		if(!Opened && !Open()) return 0;

		return OnRead(bs);
	}

	// 注册回调函数
	virtual void Register(TransportHandler handler, void* param = NULL)
	{
		// 特别是接口要检查this指针
		assert_ptr(this);

		_handler	= handler;
		_param		= param;
	}

#if DEBUG
	virtual string ToString() { return "ITransport"; }
#else
	virtual string ToString() { return ""; }
#endif

protected:
	virtual bool OnOpen() { return true; }
	virtual void OnClose() { }
	virtual bool OnWrite(const ByteArray& bs) = 0;
	virtual uint OnRead(ByteArray& bs) = 0;

	// 是否有回调函数
	bool HasHandler() { return _handler != NULL; }

	// 引发数据到达事件
	virtual uint OnReceive(ByteArray& bs, void* param)
	{
		if(_handler) return _handler(this, bs, _param, param);

		return 0;
	}
};

// 数据口包装
class PackPort : public ITransport
{
private:
	//ITransport*	_port;

public:
	ITransport*	Port;	// 传输口

	virtual ~PackPort()
	{
		if(Port) Port->Register(NULL);
		delete Port;
	}

	virtual void Set(ITransport* port)
	{
		if(Port) Port->Register(NULL);

		Port = port;

		if(Port) Port->Register(OnPortReceive, this);
	}

	virtual string ToString() { return "PackPort"; }

protected:
	virtual bool OnOpen() { return Port->Open(); }
    virtual void OnClose() { Port->Close(); }

    virtual bool OnWrite(const ByteArray& bs) { return Port->Write(bs); }
	virtual uint OnRead(ByteArray& bs) { return Port->Read(bs); }

	static uint OnPortReceive(ITransport* sender, ByteArray& bs, void* param, void* param2)
	{
		assert_ptr(param);

		//PackPort* pp = (PackPort*)param;
		PackPort* pp = dynamic_cast<PackPort*>((PackPort*)param);
		return pp->OnReceive(bs, param2);
	}
};

#endif
