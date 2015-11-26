#include "ShunCom.h"

ShunCom::ShunCom()
{
	Led		= NULL;
}

void ShunCom::Init(ITransport* port, Pin rst)
{
	assert_ptr(port);

	Set(port);
	MaxSize	= 82;

	if(rst != P0) Reset.Init(rst, true);
}

bool ShunCom::OnOpen()
{
	debug_printf("\r\nShunCom::Open \r\n");

    debug_printf("    Sleep : ");
	Sleep.Open();
    debug_printf("    Config: ");
	Config.Open();
    debug_printf("    Reset : ");
	Reset.Open();
    debug_printf("    Power : ");
	Power.Open();

	Power	= true;
	Sleep	= false;
	Config	= false;
	Reset	= false;

	debug_printf("Power=%d Sleep=%d Config=%d Reset=%d \r\n", Power.Read(), Sleep.Read(), Config.Read(), Reset.Read());

	Port->MinSize	= MinSize;

	return PackPort::OnOpen();
}

void ShunCom::OnClose()
{
	Power	= false;
	Reset	= true;

	Power.Close();
	Sleep.Close();
	Config.Close();

	Reset.Close();

	PackPort::OnClose();
}

void ShunCom::ShowConfig()
{
	if(!Open()) return;

	Config	= true;
	Sys.Sleep(3000);

	byte buf[] = { 0xFE, 0x00, 0x21, 0x15, 0x34 };
	Write(CArray(buf));

	Sys.Sleep(300);

	ByteArray rs;
	Read(rs);
	rs.Show(true);
}

// 模块进入低功耗模式时需要处理的事情
void ShunCom::ChangePower(int level)
{
	//Power	= false;

	Sleep	= true;
	Config	= false;

	Reset	= true;

	//Power* pwr	= dynamic_cast<Power*>(Port);
	//if(pwr) pwr->ChangePower(level);
	PackPort::OnClose();
}

bool ShunCom::OnWrite(const Array& bs)
{
	//Led = !Led;

	return PackPort::OnWrite(bs);
}

// 引发数据到达事件
uint ShunCom::OnReceive(Array& bs, void* param)
{
	//Led = !Led;
	if(Led) Led->Write(1000);

	return ITransport::OnReceive(bs, param);
}
