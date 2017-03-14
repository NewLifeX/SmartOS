#ifndef __ITransport_H__
#define __ITransport_H__

#include "Kernel\Sys.h"

class ITransport;

// 传输口数据到达委托。传入数据缓冲区地址和长度，如有反馈，仍使用该缓冲区，返回数据长度
typedef uint (*TransportHandler)(ITransport* port, Buffer& bs, void* param, void* param2);

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

	ushort	MinSize;	// 数据包最小大小
	ushort	MaxSize;	// 数据包最大大小

	// 初始化
	ITransport();
	// 析构函数确保关闭
	virtual ~ITransport();

	// 打开传输口
	bool Open();
	// 关闭传输口
	void Close();

	// 发送数据
	bool Write(const Buffer& bs);
	bool Write(const Buffer& bs, const void* opt);
	// 接收数据
	uint Read(Buffer& bs);

	// 注册回调函数
	virtual void Register(TransportHandler handler, void* param = nullptr);

protected:
	virtual bool OnOpen() { return true; }
	virtual void OnClose() { }
	virtual bool OnWrite(const Buffer& bs) = 0;
	virtual bool OnWriteEx(const Buffer& bs, const void* opt);
	virtual uint OnRead(Buffer& bs) = 0;

	// 是否有回调函数
	bool HasHandler() { return _handler != nullptr; }

	// 引发数据到达事件
	virtual uint OnReceive(Buffer& bs, void* param);
};

// 数据口包装
class PackPort : public ITransport
{
private:

public:
	ITransport*	Port;	// 传输口

	PackPort();
	virtual ~PackPort();

	virtual void Set(ITransport* port);

	//virtual const String ToString() const { return String("PackPort"); }

protected:
	virtual bool OnOpen();
    virtual void OnClose();

    virtual bool OnWrite(const Buffer& bs);
	virtual uint OnRead(Buffer& bs);

	static uint OnPortReceive(ITransport* sender, Buffer& bs, void* param, void* param2);
};

#endif
