#include "Message.h"

// 初始化消息，各字段为0
void Message::Init()
{
	memset(this, 0, sizeof(Message));
}

// 分析数据，转为消息。负载数据部分将指向数据区，外部不要提前释放内存
void Message::Parse(byte* buf, uint len)
{
	assert_ptr(buf);
	assert_param(len > 0);

	// 消息至少4个头部字节和2个校验字节，没有负载数据的情况下
	const int headerSize = 4 + 2;
	if(len < headerSize) return NULL;

	Message* msg = (Message*)buf;
	if(len > headerSize)
	{
		msg->Length = len - headerSize;
		msg->Data = buf + headerSize;
	}

	return msg;
}

// 验证消息校验和是否有效
bool Verify()
{
	if(!data) len == 0;

	return Length == len;
}

// 构造控制器
Controller::Controller(ITransport* port)
{
	assert_ptr(port);

	// 注册收到数据事件
	port->Register(OnReceive, this);

	_port = port;
}

void Controller::OnReceive(ITransport* transport, byte* buf, uint len, void* param)
{
	assert_ptr(param);

	Controller* control = (Controller*)param;
	control->Process(byte* buf, uint len);
}

void Controller::Process(byte* buf, uint len)
{
}
