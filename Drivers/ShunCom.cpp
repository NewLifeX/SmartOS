#include "ShunCom.h"
#include "CheckSum.h"

class ShunComMessage
{
public:
	byte 		Frame;		//帧头
	byte		Length;		//数据长度
	uint		Code;		//操作码
	uint		CodeKind;  	//操类型
	uint		DataLength;	//负载数据长度
	byte   		Data[64];	// 负载数据部分
	byte		Checksum;	//异或校验、从数据长度到负载数据尾
public:
	//ShunComMessage(uint code,uint codeKind);
	bool Read(Stream& ms);
	// 写入指定数据流
	void Write(Stream& ms) const;
	// 验证消息校验码是否有效
	bool Valid();
	// 显示消息内容
	void Show() const;
};

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

	debug_printf("Power=%d Sleep=%d Config=%d Reset=%d MinSize=%d\r\n", Power.Read(), Sleep.Read(), Config.Read(), Reset.Read(), MinSize);

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
	debug_printf("Config=%d\n",Config.Read());
	Sys.Sleep(2000);
	Config	= false;

	//读取ShunCom模块配置指令
	byte buf[] = {0xFE, 0x00, 0x21, 0x15, 0x34};
	Write(CArray(buf));

	Sys.Sleep(300);

	ByteArray rs;
	Read(rs);
    debug_printf("ShunCom配置信息\n");
	rs.Show(true);
}

void ShunCom::SetDeviceMode(byte kind)
{
	if(!EnterSetMode()) return;

	debug_printf("设置设备模式\n");

	byte buf[] = {0xFE,0x05, 0x21,0x09,0x87,0x00,0x00,0x01,0x01,0xAA };
	Write(CArray(buf));
    OutSetMode();
}

// 设置无线频点，注意大小端，ShunCom是小端存储
void ShunCom::SetChannel(int kind)
{
	if(!EnterSetMode()) return;

	byte buf[] = { 0xFE,0x08,0x21,0x09,0x84,0x00,0x00,0x04,0x00,0x00,0x80,0x00,0x3A};
	Write(CArray(buf));
    OutSetMode();
}

void ShunCom::SetPanID(int kind)
{
	if(!EnterSetMode()) return;

	debug_printf("配置信息SetPanID\r\n");

	byte buf[] = { 0xFE,0x06,0x21,0x09,0x83,0x00,0x00,0x02,0x55,0x55,0xAF};
	Write(CArray(buf));
    OutSetMode();
}

//设置发送模式00为广播、01为主从模式、02为点对点模式
void  ShunCom::SetSendMode(byte mode)
{
	if(!EnterSetMode()) return;

	debug_printf("设置发送模式\n");

	byte buf[] = {0xFE,0x05, 0x21,0x09,0x03,0x04,0x00,0x01,mode,0x2A};
	Write(CArray(buf));
    OutSetMode();
}

//进入配置模式
bool ShunCom:: EnterSetMode()
{
	if(!Open()) return false;

	Sys.Sleep(2000);

	Config	= true;
	Sys.Sleep(2000);
	Config	= false;
	ByteArray rs1;

	while(true)
	{
		ByteArray rs1;
		Read(rs1);
		if(rs1.Length() == 0) break;
	}

	//读取ShunCom模块配置指令
	byte buf[] = { 0xFE, 0x00, 0x21, 0x15, 0x34 };
	Write(CArray(buf));

	Sys.Sleep(300);

	ByteArray rs;
	Read(rs);
    debug_printf("ShunCom配置信息\n");
	rs.Show(true);

	return true;
}

//退出配置模式
void ShunCom::OutSetMode()
{
	if(!Open()) return;

	debug_printf("重启模块\n");
	byte buf[] = {0xFE,0x01,0x41,0x00,0x00,0x40};
	Write(CArray(buf));
	// Config	= false;
}

//读取配置信息
void  ShunCom::ConfigMessage(ByteArray& array)
{
	//读取ShunCom模块配置指令
	byte buf[] = {0xFE, 0x00, 0x21, 0x15, 0x34 };
	Write(CArray(buf));

	Sys.Sleep(300);

	Read(array);
    debug_printf("ShunCom配置信息\n");
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

// 引发数据到达事件
uint ShunCom::OnReceive(Array& bs, void* param)
{
	if(Led) Led->Write(1000);

	return ITransport::OnReceive(bs, param);
}

void ShunComMessage::Write(Stream& ms) const
{
	assert_ptr(this);

	ms.Write(Frame);
	ms.Write(Length);
	ms.Write(Code);
	ms.Write(CodeKind);
	ms.Write(DataLength);
	ms.Write(CArray(Data));
	ms.Write(Checksum);
}
