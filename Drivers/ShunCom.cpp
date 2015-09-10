#include "ShunCom.h"

ShunCom::ShunCom()
{
	Port = NULL;
}

ShunCom::~ShunCom()
{
	delete Port;
	Port = NULL;
}

void ShunCom::Init(ITransport* port, Pin rst)
{
	assert_ptr(port);

	Port = port;

	if(rst != P0) Reset.Set(rst);
}

bool ShunCom::OnOpen()
{
	Power.Open();
	Sleep.Open();
	Config.Open();

	Power	= true;
	Sleep	= false;
	Config	= false;

	Reset.Open();
	Reset	= true;
	Sys.Delay(100);
	Reset	= false;
	Sys.Delay(100);
	Reset	= true;

	return Port->Open();
}

void ShunCom::OnClose()
{
	Power.Close();
	Sleep.Close();
	Config.Close();

	Reset.Close();

	Port->Close();
}

bool ShunCom::OnWrite(const byte* buf, uint len) { return Port->Write(buf, len); }
uint ShunCom::OnRead(byte* buf, uint len) { return Port->Read(buf, len); }

// 注册回调函数
void ShunCom::Register(TransportHandler handler, void* param)
{
	ITransport::Register(handler, param);

	if(handler)
		Port->Register(OnPortReceive, this);
	else
		Port->Register(NULL);
}

uint ShunCom::OnPortReceive(ITransport* sender, byte* buf, uint len, void* param)
{
	assert_ptr(param);

	ShunCom* zb = (ShunCom*)param;
	return zb->OnReceive(buf, len);
}
