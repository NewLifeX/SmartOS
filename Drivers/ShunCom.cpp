#include "ShunCom.h"

ShunCom::ShunCom()
{
	Led		= NULL;
}

void ShunCom::Init(ITransport* port, Pin rst)
{
	assert_ptr(port);

	Set(port);
	//MaxSize	= 82;
	MaxSize	= 64;

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
	Sys.Sleep(2000);
	Config	= false;
	

	//读取Zibeer模块配置指令
	byte buf[] = { 0xFE, 0x00, 0x21, 0x15, 0x34 };
	Write(CArray(buf));

	Sys.Sleep(300);

	ByteArray rs;
	Read(rs);
    debug_printf("Zibeer配置信息\n");
	rs.Show(true);
}

void ShunCom:: SetDeviceMode(byte kind)
{
	if(!Open()) return;
	EnterSetMode();
	byte buf[] = { 0xFE, 0x05, 0x21, 0x09, 0x87,0x00,0x00,kind,0xAB };
	Write(CArray(buf));
    OutSetMode();
}
//设置无线频点，注意大小端，Zibeer是小端存储
void ShunCom::SetChannel(int kind)
{
	if(!Open()) return;
	EnterSetMode();
	byte buf[] = { 0xFE,0x08,0x21,0x09,0x84,0x00,0x00,0x04,0x00,0x08,0x00,0x00,0xA8 };
	Write(CArray(buf));
    OutSetMode();	
}
//设置发送模式00为广播、01为主从模式、02为点对点模式
void  ShunCom::SetSendMode(byte mode)
{
	if(!Open()) return;
	EnterSetMode();
	byte buf[] = {0xFE,0x05,0x21,0x09,0x03,0x04,0x00,0x01 ,mode,0x2B };
	Write(CArray(buf));
    OutSetMode();	
	
}	
//进入配置模式
void ShunCom:: EnterSetMode()
{
	if(!Open()) return;

	Config	= true;
	Sys.Sleep(2000);
	Config	= false;
	//读取Zibeer模块配置指令
	byte buf[] = { 0xFE, 0x00, 0x21, 0x15, 0x34 };
	Write(CArray(buf));
	Sys.Sleep(300);
	ByteArray rs;
	Read(rs);
    debug_printf("Zibeer配置信息\n");
	rs.Show(true);	
}
//退出配置模式
void ShunCom:: OutSetMode()
{
  if(!Open()) return;
  byte buf[] = {0xFE,0x01,0x41,0x00,0x00,0x40};
  Write(CArray(buf));	
}
//读取配置信息
void  ShunCom::ConfigMessage(ByteArray& array)
{
	if(!Open()) return;

	Config	= true;
	Sys.Sleep(2000);

	//读取Zibeer模块配置指令
	byte buf[] = { 0xFE, 0x00, 0x21, 0x15, 0x34 };
	Write(CArray(buf));

	Sys.Sleep(300);
	
	Read(array);
    debug_printf("Zibeer配置信息\n");
	array.Show(true);
	
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
