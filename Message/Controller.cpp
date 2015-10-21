#include "Controller.h"

#define MSG_DEBUG DEBUG
//#define MSG_DEBUG 0
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

uint Controller::Dispatch(ITransport* port, ByteArray& bs, void* param, void* param2)
{
	byte* buf	= bs.GetBuffer();
	uint len	= bs.Length();

	assert_ptr(buf);
	assert_ptr(param);

	Controller* control = (Controller*)param;

#if MSG_DEBUG
	/*msg_printf("TinyNet::Dispatch[%d] ", len);
	// 输出整条信息
	ByteArray(buf, len).Show(true);*/
#endif
	if(len > control->Port->MaxSize)
	{
#if MSG_DEBUG
		msg_printf("TinyNet::Dispatch[%d] ", len);
		// 输出整条信息
		ByteArray(buf, len).Show(true);
#endif
		msg_printf("数据长度 %d 超过控制器可接受最大长度 %d \r\n", len, control->Port->MaxSize);
		//assert_param2(len <= control->Port->MaxSize, "数据长度超过控制器可接受最大长度");
	}

	// 这里使用数据流，可能多个消息粘包在一起
	// 注意，此时指针位于0，而内容长度为缓冲区长度
	Stream ms(buf, len);
	while(ms.Remain() >= control->MinSize)
	{
#if MSG_DEBUG
		uint p = ms.Position();
		buf = ms.Current();
		len = ms.Remain();
#endif
		// 如果不是有效数据包，则直接退出，避免产生死循环。当然，也可以逐字节移动测试，不过那样性能太差
		if(!control->Dispatch(ms, NULL))
		{
#if MSG_DEBUG
			// 兼容性处理，如果0x00 0x01 0x02开头，则重新来一次
			if(buf[0] == 0x00 || buf[0] == 0x01 || buf[0] == 0x02 || buf[0] == 0x03)
			{
				ms.SetPosition(p + 1);
				continue;
			}
			
			msg_printf("TinyNet::DispatchError[%d] ", len);
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

bool Controller::Dispatch(Stream& ms, Message* pmsg)
{
	byte* buf = ms.Current();

	Message& msg = *pmsg;
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
		if(!Received(msg, Param)) return true;
	}

	return true;
}

bool Controller::Send(Message& msg)
{
	// 如果没有传输口处于打开状态，则发送失败
	if(!Port->Open()) return false;

	//uint len = msg.Size();

	// ms需要在外面这里声明，否则离开大括号作用域以后变量被销毁，导致缓冲区不可用
	//Stream ms(len);
	byte buf[512];
	Stream ms(buf, ArrayLength(buf));
	// 带有负载数据，需要合并成为一段连续的内存
	msg.Write(ms);
	//assert_param2(len == ms.Position(), "消息标称大小和实际大小不符");
	/*uint len = ms.Position();
	// 内存流扩容以后，指针会改变
	byte* p = ms.GetBuffer();*/

	ByteArray bs(ms.GetBuffer(), ms.Position());
	return Port->Write(bs);
}

bool Controller::Reply(Message& msg)
{
	msg.Reply = 1;

	return Send(msg);
}
