#include "ShunCom.h"

ShunCom::ShunCom() { }

void ShunCom::Init(ITransport* port, Pin rst)
{
	assert_ptr(port);

	Set(port);

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

	return PackPort::OnOpen();
}

void ShunCom::OnClose()
{
	Power.Close();
	Sleep.Close();
	Config.Close();

	Reset.Close();

	PackPort::OnClose();
}
