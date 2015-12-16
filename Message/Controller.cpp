#include "Controller.h"

//#define MSG_DEBUG DEBUG
#define MSG_DEBUG 0
#if MSG_DEBUG
	#define msg_printf debug_printf
#else
	#define msg_printf(format, ...)
#endif

// 构造控制器
Controller::Controller()
{
	Port		= NULL;
	MinSize 	= 0;
	Opened		= false;

	Received	= NULL;
	Param		= NULL;
}

Controller::~Controller()
{
	Close();

	if(Port) delete Port;

	debug_printf("TinyNet::UnInit\r\n");
}

void Controller::Open()
{
	if(Opened) return;

	assert_param2(Port, "还没有传输口呢");

	Port->MinSize	= MinSize;
	// 注册收到数据事件
	Port->Register(Dispatch, this);
	Port->Open();

	Opened = true;
}

void Controller::Close()
{
	if(!Opened) return;

	Port->Close();

	Opened = false;
}

uint Controller::Dispatch(ITransport* port, Array& bs, void* param, void* param2)
{
	TS("Controller::Dispatch");

	byte* buf	= bs.GetBuffer();
	uint len	= bs.Length();

	assert_ptr(buf);
	assert_ptr(param);

	auto control = (Controller*)param;

#if MSG_DEBUG
	msg_printf("Controller::Receive[%d] ", len);
	// 输出整条信息
	ByteArray(buf, len).Show(true);
#endif

	// 这里使用数据流，可能多个消息粘包在一起
	// 注意，此时指针位于0，而内容长度为缓冲区长度
	Stream ms((const void*)buf, len);
	while(ms.Remain() >= control->MinSize)
	{
#if MSG_DEBUG
		uint p = ms.Position();
		buf = ms.Current();
		len = ms.Remain();
#endif
		// 如果不是有效数据包，则直接退出，避免产生死循环。当然，也可以逐字节移动测试，不过那样性能太差
		if(!control->Dispatch(ms, NULL, param2))
		{
#if MSG_DEBUG
			msg_printf("Controller::Error[%d] ", len);
			// 输出整条信息
			ByteArray(buf, len).Show(true);
#endif
			break;
		}

		assert_param2(control, "控制器指针已被改变3");
		assert_ptr(control);
	}

	return 0;
}

bool Controller::Dispatch(Stream& ms, Message* pmsg, void* param)
{
	TS("Controller::DispatchMsg");

	byte* buf = ms.Current();

	auto& msg	= *pmsg;
	msg.State	= param;
	if(!msg.Read(ms)) return false;

	// 校验
	if(!msg.Valid()) return true;

	if(!Valid(msg))
	{
		/*debug_printf("消息校验未通过\r\n");
		msg.Show();
		debug_printf("\r\n");*/

		return true;
	}
    if(!msg.Valid()) return true;
	return OnReceive(msg);
}

bool Controller::Valid(const Message& msg)
{
	return true;
}

// 接收处理
bool Controller::OnReceive(Message& msg)
{
	// 外部公共消息事件
	if(Received)
	{
		if(!Received(this, msg, Param)) return true;
	}

	return true;
}

bool Controller::Send(Message& msg)
{
	return SendInternal(msg);
}

bool Controller::Reply(Message& msg)
{
	msg.Reply = 1;

	return SendInternal(msg);
}

bool Controller::SendInternal(const Message& msg)
{
	TS("Controller::SendInternal");

	// 如果没有传输口处于打开状态，则发送失败
	if(!Port->Open()) return false;

	MemoryStream ms;
	// 带有负载数据，需要合并成为一段连续的内存
	msg.Write(ms);

	Array bs(ms.GetBuffer(), ms.Position());
	return Port->Write(bs, msg.State);
}
