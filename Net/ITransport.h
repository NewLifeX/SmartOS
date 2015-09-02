#ifndef __ITransport_H__
#define __ITransport_H__

#include "Sys.h"

class ITransport;

// 传输口数据到达委托。传入数据缓冲区地址和长度，如有反馈，仍使用该缓冲区，返回数据长度
typedef uint (*TransportHandler)(ITransport* transport, byte* buf, uint len, void* param);

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
	bool Write(const byte* buf, uint len)
	{
		// 特别是接口要检查this指针
		assert_ptr(this);

		if(!Opened && !Open()) return false;

		return OnWrite(buf, len);
	}

	// 发送数据
	bool Write(const ByteArray& bs)
	{
		// 特别是接口要检查this指针
		assert_ptr(this);

		if(!Opened && !Open()) return false;

		return OnWrite(bs.GetBuffer(), bs.Length());
	}

	// 接收数据
	uint Read(byte* buf, uint len)
	{
		// 特别是接口要检查this指针
		assert_ptr(this);

		if(!Opened && !Open()) return 0;

		return OnRead(buf, len);
	}

	// 接收数据
	uint Read(ByteArray& bs)
	{
		// 特别是接口要检查this指针
		assert_ptr(this);

		if(!Opened && !Open()) return 0;

		return OnRead(bs.GetBuffer(), bs.Length());
	}

	// 注册回调函数
	virtual void Register(TransportHandler handler, void* param = NULL)
	{
		// 特别是接口要检查this指针
		assert_ptr(this);

		if(handler)
		{
			_handler	= handler;
			_param		= param;

			if(!Opened) Open();
		}
		else
		{
			_handler	= NULL;
			_param		= NULL;
		}
	}

#if DEBUG
	virtual string ToString() { return "ITransport"; }
#else
	virtual string ToString() { return ""; }
#endif

protected:
	virtual bool OnOpen() { return true; }
	virtual void OnClose() { }
	virtual bool OnWrite(const byte* buf, uint len) = 0;
	virtual uint OnRead(byte* buf, uint len) = 0;

	// 是否有回调函数
	bool HasHandler() { return _handler != NULL; }

	// 引发数据到达事件
	virtual uint OnReceive(byte* buf, uint len)
	{
		if(_handler) return _handler(this, buf, len, _param);

		return 0;
	}
};

#endif
