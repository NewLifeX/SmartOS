#include "BufferPort.h"
#include "SerialPort.h"

BufferPort::BufferPort() : Buf()
{
	Name	= "BufferPort";
	Port	= nullptr;

	Com		= COM_NONE;
	Speed	= 9600;

	Opened	= false;

	Led		= nullptr;
}

BufferPort::~BufferPort()
{
	delete Port;
	Port = nullptr;
}

bool BufferPort::Open()
{
	if(Opened) return true;

	bool isNew	= false;
	if(Port)
	{
		auto obj	= dynamic_cast<Object*>(Port);
		if(obj)
			debug_printf("%s::Open %s \r\n", Name, obj->ToString());
		else
			debug_printf("%s::Open \r\n", Name);
	}
	else
	{
		debug_printf("%s::Open COM%d %d \r\n", Name, (byte)Com + 1, Speed);

		isNew	= true;
	}

	if(Buf.Capacity() == 0)
	{
		debug_printf("未指定缓冲区大小，默认分配 256 字节！\r\n");

		Buf.SetLength(256);
		//return false;
	}
	Buf.SetLength(0);

	if(isNew && Com != COM_NONE)
	{
		auto sp	= new SerialPort(Com, Speed);
		Port	= sp;
	}

	if(!OnOpen(isNew) || Port == nullptr) return false;

	Port->Register(OnReceive, this);

	return Opened = Port->Open();
}

bool BufferPort::OnOpen(bool isNew) { return true; }

void BufferPort::Close()
{
	if(!Opened) return;

	if(Port)
	{
		Port->Close();
		//Port->Register(nullptr, nullptr);
	}

	Opened	= false;
}

uint BufferPort::OnReceive(ITransport* transport, Buffer& bs, void* param, void* param2)
{
	auto bp	= (BufferPort*)param;
	if(bp) bp->OnReceive(bs, param2);

	return 0;
}

void BufferPort::OnReceive(const Buffer& bs, void* param)
{
	if(Buf.Capacity() > 0)
	{
		Buf.SetLength(0);
		Buf.Copy(0, bs, 0, bs.Length());
	}
}
