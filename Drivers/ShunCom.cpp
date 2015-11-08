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
	/*Sys.Delay(100);
	Reset	= false;
	Sys.Delay(100);
	Reset	= true;*/

	Port->MinSize	= MinSize;

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

void ShunCom::ShowConfig()
{
	if(!Open()) return;

	Config	= true;
	Sys.Sleep(3000);

	byte buf[] = { 0xFE, 0x00, 0x21, 0x15, 0x34 };
	ByteArray bs(buf, ArrayLength(buf));
	Write(bs);

	Sys.Sleep(300);

	ByteArray rs;
	Read(rs);
	rs.Show(true);
}

// 模块进入低功耗模式时需要处理的事情
void ShunCom::ChangePower(int level)
{
	Reset	= false;

	Power	= false;
	Sleep	= false;
	Config	= false;
}

bool ShunCom::OnWrite(const ByteArray& bs)
{
	//Led = !Led;

	return PackPort::OnWrite(bs);
}

// 引发数据到达事件
uint ShunCom::OnReceive(ByteArray& bs, void* param)
{
	//Led = !Led;
	if(Led) Led->Write(1000);

	return ITransport::OnReceive(bs, param);
}
