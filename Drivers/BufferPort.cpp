#include "BufferPort.h"
#include "SerialPort.h"

BufferPort::BufferPort() : Buffer((void*)NULL, 0)
{
	Name	= "BufferPort";
	Port	= NULL;

	Com		= COM_NONE;
	Speed	= 9600;

	Opened	= false;

	Led		= NULL;
}

BufferPort::~BufferPort()
{
	delete Port;
	Port = NULL;
}

bool BufferPort::Open()
{
	if(Opened) return true;

	bool isNew	= false;
	if(Port)
	{
		debug_printf("%s::Open %s \r\n", Name, Port->ToString());
	}
	else
	{
		debug_printf("%s::Open COM%d %d \r\n", Name, (byte)Com + 1, Speed);

		isNew	= true;
	}

	if(Buffer.Capacity() == 0)
	{
		debug_printf("未指定缓冲区大小，默认分配 256 字节！\r\n");

		Buffer.SetLength(256);
		//return false;
	}
	Buffer.SetLength(0);

	if(isNew && Com != COM_NONE)
	{
		auto sp	= new SerialPort(Com, Speed);
		Port	= sp;
	}

	if(!OnOpen(isNew) || Port == NULL) return false;

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
		//Port->Register(NULL, NULL);
	}

	Opened	= false;
}

uint BufferPort::OnReceive(ITransport* transport, Array& bs, void* param, void* param2)
{
	auto bp	= (BufferPort*)param;
	if(bp) bp->OnReceive(bs, param2);

	return 0;
}

void BufferPort::OnReceive(const Array& bs, void* param)
{
	if(Buffer.Capacity() > 0)
	{
		Buffer.SetLength(0);
		Buffer.Copy(bs);
	}
}
