#include "ITransport.h"

/******************************** ITransport ********************************/

// 初始化
ITransport::ITransport()
{
	Opening		= false;
	Opened		= false;

	MinSize		= 0;
	MaxSize		= 0;

	_handler	= NULL;
	_param		= NULL;
}

// 析构函数确保关闭
ITransport::~ITransport()
{
	if(Opened) Close();

	Register(NULL);
}

// 打开传输口
bool ITransport::Open()
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
void ITransport::Close()
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
bool ITransport::Write(const Buffer& bs)
{
	// 特别是接口要检查this指针
	assert_ptr(this);

	if(!Opened && !Open()) return false;

	return OnWrite(bs);
}

// 发送数据
bool ITransport::Write(const Buffer& bs, void* opt)
{
	// 特别是接口要检查this指针
	assert_ptr(this);

	if(!Opened && !Open()) return false;

	return OnWriteEx(bs, opt);
}

// 接收数据
uint ITransport::Read(Buffer& bs)
{
	// 特别是接口要检查this指针
	assert_ptr(this);

	if(!Opened && !Open()) return 0;

	return OnRead(bs);
}

// 注册回调函数
void ITransport::Register(TransportHandler handler, void* param)
{
	// 特别是接口要检查this指针
	assert_ptr(this);

	_handler	= handler;
	_param		= param;
}

// 引发数据到达事件
uint ITransport::OnReceive(Buffer& bs, void* param)
{
	if(_handler) return _handler(this, bs, _param, param);

	return 0;
}

bool ITransport::OnWriteEx(const Buffer& bs, void* opt)
{
	return OnWrite(bs);
}

/******************************** PackPort ********************************/

PackPort::PackPort(){ Port = NULL; }

PackPort::~PackPort()
{
	if(Port) Port->Register(NULL);
	delete Port;
}

void PackPort::Set(ITransport* port)
{
	if(Port) Port->Register(NULL);
	Port = port;
	if(Port)
	{
		//MinSize	= Port->MinSize;
		MaxSize	= Port->MaxSize;

		Port->Register(OnPortReceive, this);
	}
}

bool PackPort::OnOpen() { return Port->Open(); }
void PackPort::OnClose() { Port->Close(); }

bool PackPort::OnWrite(const Buffer& bs) { return Port->Write(bs); }
uint PackPort::OnRead(Buffer& bs) { return Port->Read(bs); }

uint PackPort::OnPortReceive(ITransport* sender, Buffer& bs, void* param, void* param2)
{
	assert_ptr(param);

	//PackPort* pp = (PackPort*)param;
	PackPort* pp = dynamic_cast<PackPort*>((PackPort*)param);
	return pp->OnReceive(bs, param2);
}
