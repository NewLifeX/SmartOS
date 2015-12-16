#include "ShunCom.h"
#include "CheckSum.h"

class ShunComMessage
{
public:
	byte 		Frame;		// 帧头
	byte		Length;		// 数据长度
	ushort		Code;		// 操作码
	ushort		Kind;  		// 操类型
	ushort		Size;		// 负载数据长度
	byte   		Data[64];	// 负载数据部分
	byte		Checksum;	// 校验和、从数据长度到负载数据尾

	const int HEADERSIZE = 1 + 1 + 2 + 2 + 2;

public:
	ShunComMessage(ushort code = 0);

	bool Read(Stream& ms);
	void Write(Stream& ms) const;
	ByteArray ToArray() const;
	ByteArray ToArray(Stream& ms); 
	void Set(ushort kind, const Array& bs);
	void Set(ushort kind, byte dat);
	void Set(ushort kind, ushort dat);
	void Set(ushort kind, uint dat);

	// 验证消息校验码是否有效
	//bool Valid();
	// 显示消息内容
	//void Show() const;
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
	MaxSize		= 64;
	AddrLength	= 0;

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

	if(!AddrLength) return ITransport::OnReceive(bs, param);

	// 取出地址
	byte* addr	= bs.GetBuffer();
	Array bs2(addr + AddrLength, bs.Length() - AddrLength);
	return ITransport::OnReceive(bs2, addr);
}

bool ShunCom::OnWriteEx(const Array& bs, void* opt)
{
	if(!AddrLength || !opt) return OnWrite(bs);

	// 加入地址
	ByteArray bs2;
	bs2.Copy(opt, AddrLength);
	bs2.Copy(bs, AddrLength);

	return OnWrite(bs2);
}

// 进入配置模式
bool ShunCom::EnterConfig()
{
	if(!Open()) return false;

	Sys.Sleep(2000);

	Config	= true;
	Sys.Sleep(2000);
	Config	= false;
	ByteArray rs1;

	// 清空串口缓冲区
	while(true)
	{
		ByteArray rs1;
		Read(rs1);
		if(rs1.Length() == 0) break;
	}

	return true;
}

// 退出配置模式
void ShunCom::ExitConfig()
{
	if(!Open()) return;

	ShunComMessage msg(0x0041);
	msg.Length	= 1;
	msg.Data[0]	= 0x00;
	MemoryStream ms;
	
	auto buf = msg.ToArray(ms);
	debug_printf("ShunCom退出配置\r\n");
	buf.Show();
	Write(buf);	
    debug_printf("\r\n"); 	
	
}

// 读取配置信息
void ShunCom::ShowConfig()
{	
	ShunComMessage msg(0x1521);
	
	MemoryStream ms;
	auto buf = msg.ToArray(ms);
	debug_printf("ShunCom配置设备类型\r\n");
	buf.Show();
	Write(buf);	

	Sys.Sleep(300);

	ByteArray bs;
	Read(bs);
    debug_printf("ShunCom配置信息\r\n");
	bs.Show(true);
}

// 设置设备的类型：00代表中心、01代表路由，02代表终端
void ShunCom::SetDevice(byte kind)
{
	ShunComMessage msg(0x0921);
	msg.Set(0x0087, kind);
	MemoryStream ms;
	auto buf = msg.ToArray(ms);
	debug_printf("ShunCom配置设备类型\r\n");
	buf.Show();
	Write(buf);	
    debug_printf("\r\n"); 	
}

// 设置无线频点，注意大小端，ShunCom是小端存储
void ShunCom::SetChannel(byte chn)
{	
	ShunComMessage msg(0x0921);
	//todo 这里需要查资料核对左移公式
	msg.Set(0x0084, (uint)(0x01 << chn));
	
	MemoryStream ms;
	auto buf=msg.ToArray(ms);
	debug_printf("ShunCom配置无线频点\r\n");
	buf.Show();
	Write(buf);	
    debug_printf("\r\n"); 	
}

// 进入配置PanID,同一网络PanID必须相同
void ShunCom::SetPanID(ushort id)
{	
	ShunComMessage msg(0x0921);
	msg.Set(0x0083, id);
	
	MemoryStream ms;
	auto buf = msg.ToArray(ms);
	debug_printf("ShunCom配置PanID\r\n");
	buf.Show();
	Write(buf);	
    debug_printf("\r\n"); 	
}

// 设置发送模式00为广播、01为主从模式、02为点对点模式
void ShunCom::SetSend(byte mode)
{	
	ShunComMessage msg(0x0921);
	msg.Set(0x0403, mode);
	
	MemoryStream ms;
	auto buf = msg.ToArray(ms);
	debug_printf("ShunCom配置设备主从模式\r\n");
	buf.Show();
	Write(buf);	
    debug_printf("\r\n"); 	
}

ShunComMessage::ShunComMessage(ushort code)
{
	Frame	= 0xFE;
	Code	= code;
	Length	= 0;
}

bool ShunComMessage::Read(Stream& ms)
{
	byte magic	= ms.ReadByte();
	if(magic != 0xFE) return false;

	Frame	= magic;
	Length	= ms.ReadByte();
	Code	= ms.ReadUInt16();
	if(Length > 4)
	{
		Kind	= ms.ReadUInt16();
		Size	= __REV16(ms.ReadUInt16());
		assert_param2(2 + 2 + Size == Length, "ShunComMessage::Read");
		ms.Read(Data, 0, Size);
	}
	else if(Length > 0)
	{
		ms.Read(Data, 0, Length);
	}
	// 不做校验检查，不是很重要
	Checksum	= ms.ReadByte();

	return true;
}

void ShunComMessage::Write(Stream& ms) const
{
	byte* p	= ms.Current();
	ms.Write(Frame);
	ms.Write(Length);
	ms.Write(Code);
	if(Length > 4)
	{
		ms.Write(Kind);			     	
		ms.Write((ushort)__REV16(Size));
		ms.Write(Data, 0, Size);
	}
	else if(Length > 0)
	{
		ms.Write(Data, 0, Length);
	}
	//ms.Write(Checksum);
	// 计算校验
	byte sum	= 0;
	while(p++ < ms.Current()-1)sum^= *p;
	ms.Write(sum);
	
}



ByteArray ShunComMessage::ToArray() const
{	
	//MemoryStream ms;
	// 带有负载数据，需要合并成为一段连续的内存
	ByteArray bs;
	Stream ms(bs.GetBuffer(),64);
	//ms.SetLength(bs.Length());
	Write(ms);	
	bs.Show(true);
	return bs;
}

ByteArray ShunComMessage::ToArray(Stream& ms) 
{		
	Write(ms);
    ByteArray bs(ms.GetBuffer(), ms.Position());	
	return bs;
}
void ShunComMessage::Set(ushort kind, const Array& bs)
{
	Kind	= kind;
	bs.CopyTo(Data);

	Length	= 2 + 2 + bs.Length();
}

void ShunComMessage::Set(ushort kind, byte dat)
{
	Kind	= kind;
	Data[0]	= dat;
	Size	= 1;
	Length	= 2 + 2 + Size;
}

void ShunComMessage::Set(ushort kind, ushort dat)
{
	Kind	= kind;
	Data[0]	= dat;
	Data[1]	= dat >> 8;
	Size	= 2;
	Length	= 2 + 2 + Size;
}

void ShunComMessage::Set(ushort kind, uint dat)
{
	Kind	= kind;
	Data[0]	= dat;
	Data[1]	= dat >> 8;
	Data[2]	= dat >> 16;
	Data[3]	= dat >> 24;
	Size	= 4;
	Length	= 2 + 2 + Size;
}
