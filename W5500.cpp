
#include "W5500.h"

/*
硬件设置部分
引脚 PMODE[2:0]
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


// 数据帧格式
// 2byte Address + 1byte CONFIG_Phase + nbyte Data Phase

// 可变数据长度模式下，SCSn 拉低代表W5500的SPI 帧开始（地址段），拉高表示帧结束   （可能跟SPI NSS 不一样）
	class ByteStruct
	{
	public:
		void Init(byte data = 0) { *(byte*)this = data; }
		byte ToByte() { return *(byte*)this; }
	};
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


W5500::W5500() { Init(); };

W5500::W5500(Spi* spi, Pin irq)
{
	Init();
	Init(spi, irq);
}

void W5500::Reset()
{
	if(nRest)
	{
		*nRest = false;		// 低电平有效
		Sys.Delay(600);		// 最少500us
		*nRest = true;
	}
}

void W5500::Init()
{
	_spi = NULL;
	nRest = NULL;
	memset(&General_reg,0,sizeof(General_reg));
	memset(&_socket,NULL,sizeof(_socket)/sizeof(_socket[0]));
	PhaseOM = 0x00;
}

void W5500::Init(Spi* spi, Pin irq)
{
	if(irq != P0)
	{	
		debug_printf("W5500::Init IRQ=P%c%d\r\n",_PIN_NAME(irq));
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
	
	CONFIG_Phase config_temp;
	config_temp.BSB = 0x00;		// 寄存器区
	fra.Config = config_temp.ToByte();
	
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
	config_temp.Init(frame.Config);
	config_temp.OM = PhaseOM;
	config_temp.RWB = 1;
	frame.Config = config_temp.ToByte();
	_spi->Write(frame.Config);
	
	for(uint i = 0;i < frame.Data.Length;i ++)
	{
		_spi->Write(frame.Data.Read<byte>());
	}
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
	config_temp.Init(frame.Config);
	config_temp.OM = PhaseOM;
	config_temp.RWB = 0;
	frame.Config = config_temp.ToByte();
	_spi->Write(frame.Config);

	for(uint i = 0;i < length; i++)
		frame.Data.Write<byte>(_spi->Write(0xff));
		
	return true;
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





