#include "Zigbee.h"

Zigbee::Zigbee()
{
	_port = NULL;
}

Zigbee::Zigbee(ITransport* port, Pin rst)
{
	Init(port);
}

Zigbee::~Zigbee()
{
	delete _port;
	_port = NULL;

	/*if(_rst) delete _rst;
	_rst = NULL;*/
}

void Zigbee::Init(ITransport* port, Pin rst)
{
	assert_ptr(port);

	_port = port;

	//_rst = NULL;
	if(rst != P0)
	{
		//_rst = new OutputPort(rst);
		_rst.Set(rst).Open();
		Reset();
	}
}

void Zigbee::Reset(void)
{
	if(_rst.Empty()) return;

	_rst = true;
	Sys.Delay(100);
	_rst = false;
	Sys.Delay(100);
	_rst = true;
}

// 注册回调函数
void Zigbee::Register(TransportHandler handler, void* param)
{
	ITransport::Register(handler, param);

	if(handler)
		_port->Register(OnPortReceive, this);
	else
		_port->Register(NULL);
}

uint Zigbee::OnPortReceive(ITransport* sender, byte* buf, uint len, void* param)
{
	assert_ptr(param);

	Zigbee* zb = (Zigbee*)param;
	return zb->OnReceive(buf, len);
}
