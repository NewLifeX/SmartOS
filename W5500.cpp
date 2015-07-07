
#include "W5500.h"

/*
硬件设置部分
硬件:TCP,UDP,ICMP,IPv4,ARP,IGMP,PPPoE

引脚 PMODE[2:0]---- 也可以通过 PHYCFGR 寄存器进行配置
	000		10BI半双工，关闭自动协商
	001		10BI全双工，关闭自动协商
	010		100BI半双工，关闭自动协商
	011		100BI全双工，关闭自动协商
	100		100BI半双工，启用自动协商
	101		未启用
	110		未启用
	111		所有功能，启用自动协商
RSTn	重置	（低电平有效，低电平时间至少500us才能生效）
InTn	中断输出	（低电平有效）--- 程序中 IRQ 引脚
支持SPI 模式0和模式3
*/

/*
内部结构
一个通用寄存器区
	0x0000-0x0039	独立寻址，BSB选区
	IP MAC 等整体信息
八个Socket寄存器区
	0x0000-0x0030	独立寻址，BSB选区
8个Socket收发缓存	总缓存32K
	发缓存一共16K(0x0000-0x3fff)  	初始化分配为 每个Socket 2K
	收缓存一共16K(0x0000-0x3fff)  	初始化分配为 每个Socket 2K
	再分配时  收发都不可越16K 大边界
*/

	class ByteStruct
	{
	public:
		void Init(byte data = 0) { *(byte*)this = data; }
		byte ToByte() { return *(byte*)this; }
	};
	
// 数据帧格式
// 2byte Address + 1byte CONFIG_Phase + nbyte Data Phase
// 可变数据长度模式下，SCSn 拉低代表W5500的SPI 帧开始（地址段），拉高表示帧结束   （可能跟SPI NSS 不一样）

	// 位域 低位在前
	// 配置寄存器0x00
	typedef struct : ByteStruct
	{
		byte OM:2;		// SPI 工作模式
						// 00	可变数据长度模式，N字节数据段（1<=N）
						// 01	固定数据长度模式，1字节数据长度（N=1）
						// 10	固定数据长度模式，1字节数据长度（N=2）
						// 11	固定数据长度模式，1字节数据长度（N=4）
						// 固定模式不需要NSS （芯片NSS 接地） 可变模式SPI NSS 受控
						
		byte RWB:1;		// 0：读	1：写
		
		byte BSB:5;		// 区域选择位域		// 1个通用寄存器   8个Socket寄存器组（选择，写缓存，收缓存）
						// 00000	通用
						// XXX01	SocketXXX 选择
						// XXX10	SocketXXX 发缓存
						// XXX11	SocketXXX 收缓存
						// 其余保留， 如果选择了保留的将会导致W5500故障
	}CONFIG_Phase;

	
//通用寄存器结构
	typedef struct : ByteStruct
	{
		byte Reserved:1;
		byte FARP:1;		// 强迫ARP		0 关闭，1开启	强迫ARP模式下，无论是否发送数据都会强迫ARP请求
		byte Reserved2:1;
		byte PPPoE:1;		// PPPoE 		0 关闭，1开启
		byte PB:1;			// Ping 屏蔽位	1 ping不响应，0 ping 有响应
		byte WOL:1;			// 网络唤醒 	0关闭，1开启 （收到唤醒包发生中断）
		byte Reserved3:1;
		byte RST:1;			// 软件复位 1 复位
	}T_MR;
	
	// IR 不为零中断
	// IMR 为零屏蔽中断
	typedef struct : ByteStruct		// IMR 结构一样 
	{
		byte Reserved:4;
		byte MP:1;			// 收到网络唤醒包
		byte PPPoE:1;		// PPPoE 连接不可达
		byte UNREACH:1;		// 目标不可达
		byte CONFLICT:1;	// IP冲突
	}T_IR;
	
	// PHY 配置
	typedef struct : ByteStruct		// IMR 结构一样 
	{
		byte LNK:1;			// 连接状态 1：已连接， 0：连接断开
		byte SPD:1;			// 速度状态 1:100Mpbs， 0:10Mbps	【只读】
		byte DPX:1;			// 双工状态 1：全双工， 0：半双工 	【只读】
		byte OPMDC:3;		// 如下OPMD
		byte OPMD:1;		// 1：通过OPMDC配置，0：通过硬件引脚
							// 配置引脚 PMODE[2:0]---- 也可以通过 PHYCFGR 寄存器进行配置
							// 000		10BI半双工，关闭自动协商
							// 001		10BI全双工，关闭自动协商
							// 010		100BI半双工，关闭自动协商
							// 011		100BI全双工，关闭自动协商
							// 100		100BI半双工，启用自动协商
							// 101		未启用
							// 110		未启用
							// 111		所有功能，启用自动协商
							
		byte RST:1;			// 重启 为0时，内部PHY 重启
							// 重启后为1
	}T_PHYCFGR;
	
	
W5500::W5500() { Init(); }

W5500::W5500(Spi* spi, Pin irq)
{
	Init();
	Init(spi, irq);
}

W5500::~W5500() { }


void W5500::Reset()
{
	if(nRest)
	{
		*nRest = false;		// 低电平有效
		Sys.Delay(600);		// 最少500us
		debug_printf("硬件复位 \r\n");
		*nRest = true;
	}
	SoftwareReset();
	//Sys.Sleep(1500);
}

void W5500::Init()
{
	_spi = NULL;
	nRest = NULL;
	memset(&General_reg,0,sizeof(General_reg));
	memset(&_socket,NULL,sizeof(_socket));
	PhaseOM = 0x00;
}

void W5500::Init(Spi* spi, Pin irq)
{
	if(irq != P0)
	{	
		debug_printf("W5500::Init IRQ = P%c%d\r\n",_PIN_NAME(irq));
		_IRQ.ShakeTime = 0;
		_IRQ.Floating = false;
		_IRQ.PuPd = InputPort::PuPd_UP;
		_IRQ.Set(irq);
		_IRQ.Register(OnIRQ, this);
	}
	if(spi)
		_spi = spi;
	else 
	{
		debug_printf("SPI ERROR\r\n");
		return;
	}
	_spi->Open();
	Reset();
	
	Frame fra;
	fra.Address = 0x0000;
	
	//CONFIG_Phase config_temp;
	//config_temp.BSB = 0x00;		// 寄存器区
	//fra.Config = config_temp.ToByte();
	fra.BSB = 0x00;
	
	ReadFrame(fra,sizeof(General_reg));
	fra.Data.Read((byte *)&General_reg,0,sizeof(General_reg));
}

bool W5500::WriteFrame(Frame& frame)
{
	SpiScope sc(_spi);
	byte temp = frame.Address>>8;	// 地址高位在前
	_spi->Write(temp);
	temp = frame.Address;
	_spi->Write(temp);
	
	CONFIG_Phase config_temp;		// 配置读写和OM
	config_temp.Init(frame.BSB<<3);
	config_temp.OM = PhaseOM;		// 类内整体
	config_temp.RWB = 1;			// 写
	//frame.Config = config_temp.ToByte();
	_spi->Write(config_temp.ToByte());
	
	w5500_printf("W5500::WriteFrame Address: 0x%04X CONFIG_Phase: 0x%02X\r\n",frame.Address,config_temp.ToByte());
	uint len = frame.Data.Position();		// 获取数据流内有效数据长度
	w5500_printf("Data.Length: %d  ",len);
	w5500_printf("Data:0x");
	
	frame.Data.SetPosition(0);				// 移动游标到头部
	for(uint i = 0;i < len;i ++)
	{
		byte temp = frame.Data.Read<byte>();
		_spi->Write(temp);
		w5500_printf("-%02X",temp);
	}
	w5500_printf("\r\n\r\n");
	return true;
}

bool W5500::ReadFrame(Frame& frame,uint length)
{
	SpiScope sc(_spi);
	byte temp = frame.Address>>8;	// 地址高位在前
	_spi->Write(temp);
	temp = frame.Address;
	_spi->Write(temp);
	
	CONFIG_Phase config_temp;		// 配置读写和OM
	config_temp.Init(frame.BSB<<3);
	config_temp.OM = PhaseOM;		// 类内配置
	config_temp.RWB = 0;			// 读
	//frame.Config = config_temp.ToByte();
	_spi->Write(config_temp.ToByte());

	w5500_printf("W5500::ReadFrame Address: 0x%04X CONFIG_Phase: 0x%02X\r\n",frame.Address,config_temp.ToByte());
	w5500_printf("Read Length: %d  Data: 0x",length);
	for(uint i = 0;i < length; i++)
	{
		byte temp = _spi->Write(0x00);
		w5500_printf("-%02X",temp);
		frame.Data.Write<byte>(temp);
	}
	w5500_printf("\r\n\r\n");
	frame.Data.SetPosition(0);
	return true;
}

void W5500::SoftwareReset()
{
	debug_printf("软件复位 \r\n");
	Frame frame;
	frame.Address = 0x00000;
	frame.BSB =  0x00;	// 直接赋值简单暴力
	
	T_MR mr;
	mr.Init();
	mr.RST = 1 ;
	frame.Data.Write<byte>(mr.ToByte());
	
	WriteFrame(frame);
}

bool W5500::SetMac(MacAddress& mac)		// 本地MAC
{
	Frame frame;
	frame.Address = (ushort)((uint)General_reg.SHAR - (uint)&General_reg);
	frame.BSB =  0x00;	// 通用寄存器区
	// 通用寄存器备份一份
	memcpy(General_reg.SHAR,&mac.Value,6);
	
	frame.Data.Write<ulong>(mac.Value);
	frame.Data.SetPosition(frame.Data.Position() - 2);	// 多了2字节 这里做个处理
	
	WriteFrame(frame);
	
	ReadFrame(frame,6);
	MacAddress mac2(frame.Data.Read<ulong>());
	if(mac2 == mac)
	{
		w5500_printf("MAC设置成功！\r\n");
		return true;
	}
	return false;
}







byte W5500::GetSocket()
{
	for(byte i = 0;i < 8;i ++)
	{
		if(_socket[i] == NULL)return i ;
	}
	debug_printf("没有空余的Socket可用了 !\r\n");
	return 0xff;
}

void W5500::Register(byte Index,HardwareSocket* handler)
{
	if(_socket[Index] == NULL)
		{
			debug_printf("Index: %d 被注册 !\r\n",Index);
			_socket[Index] = handler;
		}
	else
		_socket[Index] = NULL;
}
// irq 中断处理部分
void W5500::OnIRQ(Pin pin, bool down, void* param)
{
	if(!down)return;
	W5500* send = (W5500*)param;
	send->OnIRQ();
}

void W5500::OnIRQ()
{
}

/******************************************************************/

HardwareSocket::HardwareSocket(W5500* thard)
{
	if(!thard)
	{
		_THard = thard;
		Index = _THard->GetSocket();
		if(Index < 8)
			_THard->Register(Index , this);
	}
}

HardwareSocket::~HardwareSocket()
{
}





