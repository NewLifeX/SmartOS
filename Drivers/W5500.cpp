#include "W5500.h"

#define NET_DEBUG DEBUG

#if NET_DEBUG==1
	#define w5500_printf debug_printf
#else
	__inline void w5500_printf( const char *format, ... ) {}
#endif

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
RSTn	重置		（低电平有效，低电平时间至少500us才能生效）
InTn	中断输出	（低电平有效）--- 程序中 IRQ 引脚
支持SPI 模式0和模式3 自动识别
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
	再分配时  收发都不可越16K大边界  地址自动按Socket编号调配
*/

class ByteStruct	// 位域基类
{
public:
	void Init(byte data = 0) { *(byte*)this = data; }
	byte ToByte() { return *(byte*)this; }
};

// 数据帧格式
// 2byte Address + 1byte TConfig + nbyte Data Phase
// 可变数据长度模式下，SCSn 拉低代表W5500的SPI 帧开始（地址段），拉高表示帧结束   （可能跟SPI NSS 不一样）

// 必须设定为1字节对齐，否则offsetof会得到错误的位置
#pragma pack(push)	// 保存对齐状态
// 强制结构体紧凑分配空间
#pragma pack(1)

// 通用寄存器结构
typedef struct
{
	byte MR;			// 模式			0x0000
	byte GAR[4];		// 网关地址		0x0001
	byte SUBR[4];		// 子网掩码		0x0005
	byte SHAR[6];		// 源MAC地址		0x0009
	byte SIPR[4];		// 源IP地址		0x000f
	ushort INTLEVEL;	// 低电平中断定时器寄存器	0x0013
	byte IR;			// 中断寄存器				0x0015
	byte IMR;			// 中断屏蔽寄存器			0x0016
	byte SIR;			// Socket中断寄存器			0x0017
	byte SIMR;			// Socket中断屏蔽寄存器		0x0018
	ushort RTR;			// 重试时间					0x0019
	byte RCR;			// 重试计数					0x001b
	byte PTIMER;		// PPP 连接控制协议请求定时寄存器	0x001c
	byte PMAGIC;		// PPP 连接控制协议幻数寄存器		0x001d
	byte PHAR[6];		// PPPoE 模式下目标 MAC 寄存器		0x001e
	byte PSID[2];		// PPPoE 模式下会话 ID 寄存器		0x0024
	byte PMRU[2];		// PPPoE 模式下最大接收单元			0x0026
	byte UIPR[4];		// 无法抵达 IP 地址寄存器【只读】	0x0028
	byte UPORTR[2];		// 无法抵达端口寄存器【只读】		0x002c
	byte PHYCFGR;		// PHY 配置寄存器					0x002e
	//byte VERSIONR		// 芯片版本寄存器【只读】			0x0039	// 地址不连续
}TGeneral;			// 只有一份 所以直接定义就好

typedef struct
{
	byte MR;		//0x0000  	// Socket 模式寄存器
	byte CR;		//0x0001  	// 配置寄存器 	【较为特殊】【只写，读为0x00】
	byte IR;		//0x0002  	// 中断寄存器	 写1清0？？
	byte SR;		//0x0003  	// 状态寄存器	【只读】
	byte PORT[2];	//0x0004  	// TCP UDP 模式下端口号  OPEN之前配置好
	byte DHAR[6];	//0x0006  	// 目的MAC,SEND_MAC使用;CONNECT/SEND 命令时ARP获取到的MAC
	byte DIPR[4];	//0x000c  	// 目标IP地址
	byte DPORT[2];	//0x0010  	// 目标端口
	byte MSSR[2];	//0x0012  	// TCP UDP 模式下 MTU 最大传输单元大小  默认最大值
									// TCP:1460; UDP:1472; MACRAW:1514;
									// MACRAW 模式时 由于MTU 不在内部处理，默认MTU将会生效
									// PPPoE 模式下 略
									// TCP UDP 模式下，传输数据比 MTU大时，数据将会自动划分成默认MTU 单元大小
	byte Reserved;	//0x0014
	byte TOS;		//0x0015  	// IP包头 服务类型 	OPEN之前配置
	byte TTL;		//0x0016  	// 生存时间 TTL 	OPEN之前配置
	byte Reserved2[7];	//0x0017  	-  0x001d
	byte RXBUF_SIZE;//0x001e  	// 接收缓存大小   1 2 4 8 16  单位KByte
	byte TXBUF_SIZE;//0x001f  	// 发送缓存大小   1 2 4 8 16  单位KByte
	byte TX_FSR[2];	//0x0020  	// 空闲发送寄存器大小
	byte TX_RD[2];	//0x0022  	// 发送读缓存指针
	byte TX_WR[2];	//0x0024  	// 发送写缓存指针
	byte RX_RSR[2];	//0x0026  	// 空闲接收寄存器大小
	byte RX_RD[2];	//0x0028  	// 发送读缓存指针
	byte RX_WR[2];	//0x002a  	// 发送写缓存指针
	byte IMR;		//0x002c  	// 中断屏蔽寄存器  结构跟IR一样 0屏蔽  1不屏蔽
	byte FRAG[2];	//0x002d  	// IP包头 分段部分  分段寄存器

	byte KPALVTR;	//0x002f  	// 只在TCP模式下使用  在线时间寄存器  单位：5s
									// 为0 时  手动SEND_KEEP
									// > 0 时  忽略SEND_KEEP操作
}TSocket;

#pragma pack(pop)	// 恢复对齐状态

// 位域 低位在前
// 配置寄存器  TConfig 结构
typedef struct : ByteStruct
{
	byte OpMode:2;		// SPI 工作模式
					// 00	可变数据长度模式，N字节数据段（1<=N）
					// 01	固定数据长度模式，1字节数据长度（N=1）
					// 10	固定数据长度模式，1字节数据长度（N=2）
					// 11	固定数据长度模式，1字节数据长度（N=4）
					// 固定模式不需要NSS （芯片NSS 接地） 可变模式SPI NSS 受控

	byte ReadWrite:1;	// 0：读	1：写

	byte Socket:3;		// Socket组，0表示通用寄存器

	byte BlockSelect:2;	// 区域选择位域		// 1个通用寄存器   8个Socket寄存器组（选择，写缓存，收缓存）
					// 00000	通用
					// XXX01	SocketXXX 寄存器
					// XXX10	SocketXXX 发缓存
					// XXX11	SocketXXX 收缓存
					// 其余保留， 如果选择了保留的将会导致W5500故障
}TConfig;

// 通用寄存器结构
// 模式寄存器
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
}T_Mode;

// IR 不为零中断
// IMR 为零屏蔽中断
typedef struct : ByteStruct		// IMR 结构一样
{
	byte Reserved:4;
	byte MP:1;			// 收到网络唤醒包
	byte PPPoE:1;		// PPPoE 连接不可达
	byte UNREACH:1;		// 目标不可达
	byte CONFLICT:1;	// IP冲突
}T_Interrupt;

// PHY 配置
typedef struct : ByteStruct
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

W5500::W5500(Spi* spi, Pin irq, OutputPort* rst)
{
	Init();
	Init(spi, irq, rst);
}

W5500::~W5500() { }

// 初始化
void W5500::Init()
{
	_Lock	= 0;
	_spi	= NULL;
	nRest	= NULL;

	//memset(&_sockets, NULL, sizeof(_sockets));
	ArrayZero(_sockets);

	PhaseOM = 0x00;
	//RX_FREE_SIZE = 0x16;
	//TX_FREE_SIZE = 0x16;

	RetryTime		= 200;
	RetryCount		= 8;
	LowLevelTime	= 0;
	PingACK			= true;
	EnableDHCP		= true;
	Opened			= false;

	const byte defip_[] = {192, 168, 1, 1};
	IPAddress defip(defip_);

	// 随机IP，取ID最后一个字节
	IP = defip;
	byte first = Sys.ID[0];
	if(first <= 1 || first >= 254) first = 2;
	IP[3] = first;

	Mask = IPAddress(255, 255, 255, 0);
	//Mask = 0x00FFFFFF;
	DHCPServer = Gateway = DNSServer = defip;

	MacAddress& mac = Mac;
	// 随机Mac，前三个字节取自YWS的ASCII，最后3个字节取自后三个ID
	mac[0] = 'P'; mac[1] = 'M'; //mac[2] = 'W'; mac[3] = 'L';
	for(int i=0; i< 4; i++)
		mac[2 + i] = Sys.ID[3 - i];
}

// 初始化
void W5500::Init(Spi* spi, Pin irq, OutputPort* rst)
{
	assert_ptr(spi);

	if(rst) nRest = rst;
	if(irq != P0)
	{
		// 中断引脚初始化
		debug_printf("\r\nW5500::Init IRQ = P%c%d\r\n", _PIN_NAME(irq));
		_IRQ.ShakeTime	= 0;
		_IRQ.Floating	= false;
		_IRQ.PuPd		= InputPort::PuPd_UP;
		_IRQ.Set(irq).Config();
		_IRQ.Register(OnIRQ, this);
	}

	_spi = spi;
}

bool W5500::Open()
{
	if(Opened) return true;

	_spi->Open();

	// 复位
	Reset();

	// 读硬件版本
	byte ver = ReadByte(0x0039);
	debug_printf("Hard Vision: %02X\r\n", ver);

	// 读所有寄存器
	TGeneral gen;
	ByteArray bs((byte*)&gen, sizeof(gen));
	ReadFrame(0, bs);

	// 修改各个字段值
	Gateway.ToArray().CopyTo(gen.GAR);
	Mask.ToArray().CopyTo(gen.SUBR);
	Mac.ToArray().CopyTo(gen.SHAR);
	IP.ToArray().CopyTo(gen.SIPR);

	gen.RTR = RetryTime / 10;
	gen.RCR = RetryCount;

	if(LowLevelTime > 0)
		gen.INTLEVEL = LowLevelTime;
	else
		LowLevelTime = gen.INTLEVEL;

	T_Mode mr;
	mr.Init(gen.MR);
	mr.PB = PingACK ? 0 : 1;	// 0 是有响应
	mr.FARP = 1;				// 强迫ARP模式下，无论是否发送数据都会强迫ARP请求
	gen.MR = mr.ToByte();

	T_Interrupt ir;
	ir.Init(gen.IMR);
	ir.UNREACH = 1;		// 目标不可达
	ir.CONFLICT = 1;	// IP冲突
	gen.IMR = ir.ToByte();

	gen.SIMR = 0x01;

	// 一次性全部写入
	WriteFrame(0, ByteArray((byte*)&gen, sizeof(gen)));

	//设置发送缓冲区和接收缓冲区的大小，参考W5500数据手册
	for(int i=0; i<8; i++)
	{
		WriteByte(offsetof(TSocket, RXBUF_SIZE), 0x02, i+1);	//Socket Rx memory size=2k
		WriteByte(offsetof(TSocket, RXBUF_SIZE), 0x02, i+1);	//Socket Tx mempry size=2k
	}
	//WriteByte(IMR, IMR_SENDOK | IMR_TIMEOUT | IMR_RECV | IMR_DISCON | IMR_CON);

	Opened = true;

	return true;
}

bool W5500::Close()
{
	if(!Opened) return true;

	_spi->Close();

	if(nRest)
	{
		*nRest = false;
		delete nRest;
	}
	nRest = NULL;

	Opened = false;

	return true;
}

// 复位（软硬兼施）
void W5500::Reset()
{
	if(nRest)
	{
		debug_printf("硬件复位 \r\n");
		*nRest = false;		// 低电平有效
		Sys.Delay(600);		// 最少500us
		*nRest = true;
	}

	debug_printf("软件复位 \r\n");

	T_Mode mr;
	mr.Init();
	mr.RST = 1;

	WriteByte(offsetof(TGeneral, MR), mr.ToByte());
}

// 网卡状态输出
void W5500::StateShow()
{
	TGeneral gen;
	ByteArray bs(sizeof(gen));

	// 一次性全部读出
	ReadFrame(0, bs);
	bs.CopyTo((byte*)&gen);

	debug_printf("\r\nW5500::State\r\n");

	debug_printf("MR (模式): 		0x%02X   ", gen.MR);
		T_Mode mr;
		mr.Init(gen.MR);
		debug_printf("WOL: %d   ",mr.WOL);
		debug_printf("PB: %d   ",mr.PB);
		debug_printf("PPPoE: %d   ",mr.PPPoE);
		debug_printf("FARP: %d   ",mr.FARP);
		debug_printf("\r\n");
	debug_printf("GAR (网关地址): 	%s\r\n", Gateway.ToString().GetBuffer());
	debug_printf("SUBR (子网掩码): 	%s\r\n", Mask.ToString().GetBuffer());
	debug_printf("SHAR (源MAC地址):	%s\r\n", Mac.ToString().GetBuffer());
	debug_printf("SIPR (源IP地址): 	%s\r\n", IP.ToString().GetBuffer());
	debug_printf("INTLEVEL(中断低电平时间): %d\r\n", LowLevelTime);	// 回头计算一下
	debug_printf("IMR (中断屏蔽): 	0x%02X   ", gen.IMR);
		T_Interrupt imr;
		imr.Init(gen.MR);
		debug_printf("CONFLICT: %d   ",imr.CONFLICT);
		debug_printf("UNREACH: %d   ",imr.UNREACH);
		debug_printf("PPPoE: %d   ",imr.PPPoE);
		debug_printf("MP: %d   ",imr.MP);
		debug_printf("\r\n");
	debug_printf("SIMR (Socket中断屏蔽): 0x%02X\r\n", gen.SIMR);
	debug_printf("RTR (重试时间): 	%d\r\n", RetryTime);		// 回头计算一下
	debug_printf("RCR (重试计数): 	%d 次\r\n", RetryCount);

// 暂且不输出的
//		byte PTIMER;		// PPP 连接控制协议请求定时寄存器	0x001c
//		byte PMAGIC;		// PPP 连接控制协议幻数寄存器		0x001d
//		byte PHAR[6];		// PPPoE 模式下目标 MAC 寄存器		0x001e
//		byte PSID[2];		// PPPoE 模式下会话 ID 寄存器		0x0024
//		byte PMRU[2];		// PPPoE 模式下最大接收单元			0x0026
//		byte UIPR[4];		// 无法抵达 IP 地址寄存器【只读】	0x0028
//		byte UPORTR[2];		// 无法抵达端口寄存器【只读】		0x002c
	PhyStateShow();
}

// 输出物理链路层状态
void W5500::PhyStateShow()
{
	T_PHYCFGR phy;
	phy.Init(ReadByte(offsetof(TGeneral, PHYCFGR)));
	if(phy.OPMD)
	{
		debug_printf("PHY 模式由 OPMDC 配置\r\n");
		switch(phy.OPMDC)
		{
			case 0 : debug_printf("	10BI半双工，关闭自动协商\r\n");break;
			case 1 : debug_printf("	10BI全双工，关闭自动协商\r\n");break;
			case 2 : debug_printf("	100BI半双工，关闭自动协商\r\n");break;
			case 3 : debug_printf("	100BI全双工，关闭自动协商\r\n");break;
			case 4 : debug_printf("	100BI半双工，启用自动协商\r\n");break;
			case 7 : debug_printf("	所有功能，启用自动协商\r\n");break;
		}
	}
	else
	{
		debug_printf("PHY 模式由引脚配置\r\n");
	}

	if(phy.LNK)
	{
		debug_printf("已连接 ");
		if(phy.SPD)	{ debug_printf("100Mpbs ");}
		else		{ debug_printf("10Mpbs ");}

		if(phy.DPX)	{ debug_printf("全双工");}
		else		{ debug_printf("半双工");}

		debug_printf("网络\r\n");
	}
	else
	{
		debug_printf("连接已断开\r\n");
	}
}

bool W5500::WriteByte(ushort addr, byte dat, byte socket ,byte block)
{
	SpiScope sc(_spi);

	SetAddress(addr, 1, socket, block);
	_spi->Write(dat);

	return true;
}

byte W5500::ReadByte(ushort addr, byte socket ,byte block)
{
	SpiScope sc(_spi);

	SetAddress(addr, 0, socket, block);

	return _spi->Write(0x00);
}

bool W5500::WriteByte2(ushort addr, ushort dat, byte socket ,byte block)
{
	SpiScope sc(_spi);

	SetAddress(addr, 1, socket, block);
	_spi->Write(dat);
	_spi->Write(dat>>8);

	return true;
}

ushort W5500::ReadByte2(ushort addr, byte socket ,byte block)
{
	SpiScope sc(_spi);

	SetAddress(addr, 0, socket, block);
	ushort temp = _spi->Write(0x00);
	temp += _spi->Write(0x00)<<8;
	return temp;
}

void W5500::SetAddress(ushort addr, byte rw, byte socket ,byte block)
{
	// 地址高位在前
	_spi->Write(addr >> 8);
	_spi->Write(addr & 0xFF);

	TConfig cfg;
	cfg.Socket		= socket;
	cfg.BlockSelect	= block;	// 寄存器区域选择
	cfg.OpMode		= PhaseOM;	// 类内整体
	cfg.ReadWrite	= rw;		// 读写
	_spi->Write(cfg.ToByte());
}

bool W5500::WriteFrame(ushort addr, const ByteArray& bs, byte socket ,byte block)
{
	while(_Lock != 0) Sys.Sleep(0);
	_Lock = 1;

	SpiScope sc(_spi);

	SetAddress(addr, 1, socket, block);
	_spi->Write(bs);

	_Lock = 0;
	return true;
}

bool W5500::ReadFrame(ushort addr, ByteArray& bs, byte socket ,byte block)
{
	while(_Lock != 0) Sys.Sleep(0);
	_Lock = 1;

	SpiScope sc(_spi);

	SetAddress(addr, 0, socket, block);
	_spi->Read(bs);

	_Lock = 0;
	return true;
}

// 测试PHY状态  返回是否LNK
bool W5500::CheckLink()
{
	if(!Open()) return false;

	T_PHYCFGR phy;
	phy.Init(ReadByte(offsetof(TGeneral, PHYCFGR)));

	return phy.LNK;
}

byte W5500::GetSocket()
{
	for(byte i = 1;i < 8;i ++)
	{
		if(_sockets[i] == NULL)return i;
	}
	debug_printf("没有空余的Socket可用了 !\r\n");

	return 0xff;
}

// 注册 Socket
void W5500::Register(byte Index, HardSocket* handler)
{
	if(_sockets[Index] == NULL)
	{
		debug_printf("Index: %d 被启用 !\r\n", Index);
		_sockets[Index] = handler;
	}
	else
		_sockets[Index] = NULL;
}

// irq 中断处理部分
void W5500::OnIRQ(Pin pin, bool down, void* param)
{
	if(down)return;	// 低电平中断

	W5500* send = (W5500*)param;
	send->OnIRQ();
}

void W5500::OnIRQ()
{
	// 读出中断状态
	byte dat = ReadByte(offsetof(TGeneral, IR));

	// 分析IR
	T_Interrupt ir;
	ir.Init(dat);
	if(ir.CONFLICT)
	{
		// IP 冲突
	}
	if(ir.MP)
	{
		// 收到网络唤醒包
	}
	if(ir.UNREACH)
	{
		// 读出不可达 IP 的信息
		ByteArray bs(6);
		ReadFrame(offsetof(TGeneral, UIPR), bs);	// UIPR + UPORTR
		debug_printf("IP 不可达：%s \r\n", IPEndPoint(bs).ToString().GetBuffer());
		// 处理..
	}
	// PPPoE 连接不可达
	if(ir.PPPoE)
	{

	}

	dat = ReadByte(offsetof(TGeneral, SIR));
	for(int i = 0;i < 8; i++)
	{
		if(dat & 0x01)
		{
			if(_sockets[i]) _sockets[i]->IRQ_Process();
		}
		dat >>= 1;
		if(dat == 0x00) break;
	}
	// 中断位清零 说明书说的不够清晰 有待完善
}

/****************************** 硬件Socket控制器 ************************************/

typedef struct : ByteStruct
{
	byte Protocol:4;		// Socket n 协议模式
							// 0000		Closed
							// 0001		TCP
							// 0010		UDP
							// 0100		MACRAW	【限定 Socket0 使用】
							/* MACRAW 格式												*
							 * |***PACET_INFO**|********DATA_PACKET************** |		*
							 * | PACKET_SIZE   |DEC MAC|SOU MAC| TYPE | PAYLOAD   |		*
							 * | 2BYTE		   | 6BYTE | 6BYTE | 2BYTE|46-1500BYTE|		*/

	byte UCASTB_MIP6B:1;	// 【UCASTB】【MIP6B】
							// [UCASTB]	 单播阻塞	条件：【UDP 模式】且MULTI=1
							//		0：关闭单播阻塞，1：开启单播阻塞
							// [MIP6B]	IPv6包阻塞	条件：【MACRAW 模式】
							//		0：关闭IPv6包阻塞，1：开启IPv6包阻塞

	byte ND_MC_MMB:1;		// 【ND】【MC】【MMB】
							// [ND]	使用无延时ACK	条件：【TCP 模式】
							//		0：关闭无延时ACK选项，需要RTR设定的超时时间做延时
							//		1：开启无延时ACK选项, 尽量没有延时的回复ACK
							// [MC]	IGMP版本		条件：【UDP 模式】且MULTI=1
							//		0：IGMP 版本2，
							//		1：IGMP 版本1
							// [MMB] 多播阻塞		条件：【MACRAW 模式】
							// 		0：关闭多播阻塞
							// 		1：开启多播阻塞

	byte BCASTB:1;			// 广播阻塞		条件：【MACRAW 模式】或者【UDP 模式】
							// 		0：关闭广播阻塞
							// 		1：开启广播阻塞

	byte MULTI_MFEN:1;		// 【MULTI】【MFEN】
							// [MULTI] UDP多播模式	条件：【UDP 模式】
							// 		0：关闭多播
							// 		1：开启多播
							// 注意：使用多播模式，需要DIPR  DPORT 在通过CR 打开配置命令打开之前，分别配置组播IP地址及端口号
							// [MFEN] MAC 过滤		条件：【MACRAW 模式】
							// 		0: 关闭 MAC 过滤，收到网络的所有包
							//		1：开启 MAC 过滤，只接收广播包和发给自己的包
}S_Mode;

// CR 寄存器比较特殊
// 写入后会自动清零，但命令任然在处理中，为验证命令是否完成，需检查IR或者SnSR寄存器
enum S_Command
{
	OPEN		= 0x01,	// open 之后，检测 SR 的值
						// 	MR.Protocol		SR
						// 		【CLOSE】		-
						// 		【TCP】			【SOCK_INT】0X13
						// 		【UDP】			【SOCK_UDP】0X22
						// 		【MACRAW】		【SOCK_MACRAW】0X02

	LISTEN		= 0x02,	// 只在【TCP 模式】下生效	LISTEN 会把 Socket 变成服务端，等待TCP连接
						// LISTEN 把 SOCK_INIT 变为 SOCK_LISTEN
						// 当 SOCK 被连接成功 SOCK_LISTEN 变为 SOCK_ESTABLISHE,同时 IR.CON =1
						// 		当连接非法断开 IR.TIMEOUT =1， SR = SOCK_CLOSED

	CONNECT 	= 0x04,	// 只在【TCP 模式】下生效	Socket 作为客户端  对 DIPR:DPORT 发起连接
						// 链接成功时 SR = SOCK_ESTABLISHE  IR.CON =1
						// 三种情况意味着请求失败  SR = SOCK_CLOSED
						// 	1.ARP 发生超时(IR.TIMEOUT =1)
						//  2.没有收到 SYN/ACK 数据包(IR.TIMEOUT = 1)
						//  3.收到RST数据包

	DISCON		= 0x08,	// 只在【TCP 模式】下生效	FIN/ACK 断开机制断开连接  （不分 客户端，服务端）
						// SR = SOCK_CLOSED
						// 另外 当断开请求没有收到ACK时 IR.TIMEOUT =1
						// 使用 CLOSE 命令来代替 DISCON 的话，SR = SOCK_CLOSED 不再执行 FIN/ACK 断开机制
						// 收到RST数据包 将无条件 SR = SOCK_CLOSED

	CLOSE		= 0x10,	// 关闭Socket   SR = SOCK_CLOSED

	SEND		= 0x20,	// 发送 Socket TX内存中的所有缓冲数据
						// 一般来说，先通常自动ARP获取到MAC才进行传输

	SEND_MAC	= 0x21,	// 只在【UDP 模式】下生效
						// 使用DHAR 中的MAC进行发送（不进行ARP）

	SEND_KEEP	= 0x22,	// 只在【TCP 模式】下生效	通过发送1字节在线心跳包检查连接状态
						// 如果对方不能在超时计数期内反馈在线心跳包，这个连接将被关闭并触发中断

	RECV		= 0x40,	// 通过使用接收读指针寄存器 RX_RD 来判定 接收缓存是否完成接收处理
};

//中断寄存器
typedef struct : ByteStruct
{
	byte CON:1;			// 连接成功 	SR = SOCK_ESTABLISHE 此位生效
	byte DISCON:1;		// 连接中断		接收到对方 FIN/ACK 断开机制数据包时 此位生效
	byte RECV:1;		// 无论何时，接收到对方数据包，此位生效
	byte TIMEOUT:1;		// ARP  TCP 超时被触发
	byte SEND_OK:1;		// 发送完成
	byte Reserved:3;
}S_Interrupt;

// 状态
enum S_Status
{
	// 常规状态
	SOCK_CLOSED		= 0x00,		// 关闭
	SOCK_INIT		= 0x13,     // TCP 工作模式  OPEN 后状态
	SOCK_LISTEN		= 0x14,     // TCP 服务端模式   等待 TCP 连接请求
	SOCK_ESTABLISHE	= 0x17,     // TCP 连接OK    此状态下可以 Send 或者 Recv
	SOCK_CLOSE_WAIT	= 0x1C,     // TCP 连接成功基础上 收到FIN 包，可以进行数据传输，若要全部关闭，需要使用DISCON命令
	SOCK_UDP		= 0x22,     // UDP 工作模式
	SOCK_MACRAW		= 0x02,     // MACRAW 工作模式
	// 中间状态
	SOCK_SYNSENT	= 0x15,		// CONNECT 指令后(SOCK_INIT 状态后)，SOCK_ESTABLISHE 状态前

	SOCK_SYNRECV	= 0x16,		// 收到客户端连接请求包，发送了连接应答后，连接建立成功前
								// SOCK_LISTEN 状态后，SOCK_ESTABLISHE 状态前
								// 如果超时 Sn_SR = SOCK_CLOSED  Sn_IR.TIMEOUT =1

	SOCK_FIN_WAIT	= 0x18,		// Socket 正在关闭
	SOCK_CLOSING	= 0x1A,		// Socket 正在关闭
	SOCK_TIME_WAIT	= 0x1B,		// Socket 正在关闭

	SOCK_LAST_ACK	= 0x1D,		// Socket 被动关闭状态下，等待对方断开连接请求做出回应
								// 超时或者成功收到断开请求都将 SOCK_CLOSED
};

//#define SocketWrite(P, D) Host->WriteByte(offsetof(TSocket, P), D, Index)
//#define SocketRead(P) Host->ReadByte(offsetof(TSocket, P), Index)
//#define SocketWrite2(P, D) Host->WriteByte2(offsetof(TSocket, P), D, Index)
//#define SocketRead2(P) Host->ReadByte2(offsetof(TSocket, P), Index)
//#define SocketWrites(P, D) Host->WriteFrame(offsetof(TSocket, P), D, Index)
//#define SocketReads(P, bs) Host->ReadFrame(offsetof(TSocket, P), bs, Index)

#define SocRegWrite(P, D) 	Host->WriteByte(offsetof(TSocket, P), D, Index, 0x01)
#define SocRegRead(P) 		Host->ReadByte(offsetof(TSocket, P), Index, 0x01)
#define SocRegWrite2(P, D) 	Host->WriteByte2(offsetof(TSocket, P), D, Index, 0x01)
#define SocRegRead2(P) 		Host->ReadByte2(offsetof(TSocket, P), Index, 0x01)
#define SocRegWrites(P, D) 	Host->WriteFrame(offsetof(TSocket, P), D, Index, 0x01)
#define SocRegReads(P, bs) 	Host->ReadFrame(offsetof(TSocket, P), bs, Index, 0x01)


HardSocket::HardSocket(W5500* host, byte protocol)
{
	Host = host;
	Protocol = protocol;
	if(host)
	{
		Index = host->GetSocket();
		if(Index < 8) host->Register(Index, this);
	}
	else
	{
		Index = 0xff;
	}
}

HardSocket::~HardSocket()
{
	Host->Register(Index, NULL);
}

void HardSocket::StateShow()
{
	TSocket soc;
	ByteArray bs(sizeof(soc));

	// 一次性全部读出
	Host->ReadFrame(0, bs, Index, 0x01);
	bs.CopyTo((byte*)&soc);

	debug_printf("\r\nW5500::Socket %d::State\r\n",Index);
	
	debug_printf("MR (模式): 		0x%02X\r\n", soc.MR);
		S_Mode mr;
		mr.Init(soc.MR);
		debug_printf("	Protocol:");
		switch(mr.Protocol)
		{
			case 0x00:debug_printf("		Closed\r\n");break;
			case 0x01:debug_printf("		TCP\r\n");break;
			case 0x02:debug_printf("		UDP\r\n");break;
			case 0x03:
				{
					if(Index == 0x00)debug_printf("		MACRAW");
					else	debug_printf("		ERROR！！！\r\n");
					break;
				}
			default:break;
		}
		debug_printf("	UCASTB_MIP6B:	%d\r\n",mr.UCASTB_MIP6B);
		debug_printf("	ND_MC_MMB:		%d\r\n",mr.ND_MC_MMB);
		debug_printf("	BCASTB:		%d\r\n",mr.BCASTB);
		debug_printf("	MULTI_MFEN:		%d\r\n",mr.MULTI_MFEN);
		//switch(mr.Protocol)		// 不输出这么详细
		//{
		//	case 0x00:break;
		//	case 0x01:break;
		//	case 0x02:
		//		if(mr.MULTI_MFEN)
		//		{
		//			debug_printf("UDP	");
		//			if(mr.MULTI_MFEN)
		//			{
		//				debug_printf("开启多播	");
		//				if(mr.UCASTB_MIP6B)
		//					debug_printf("开启单播阻塞	");
		//				if(mr.ND_MC_MMB)
		//					debug_printf("IGMP 版本1	");
		//				else
		//					debug_printf("IGMP 版本0	");
		//			}
		//			debug_printf("\r\n");
		//		}
		//		break;
		//	case 0x03:break;
		//}
	
	enum S_Status stat= *(enum S_Status*) &soc.SR;
	debug_printf("SR (状态):	");
	switch(stat)
	{
		// 公共
		case SOCK_CLOSED:		debug_printf("SOCK_CLOSED\r\n");break;
		case SOCK_CLOSING:		debug_printf("SOCK_CLOSING\r\n");break;
		case SOCK_SYNRECV:		debug_printf("SOCK_SYNRECV\r\n");break;
		// TCP
		case SOCK_INIT:			debug_printf("SOCK_INIT\r\n");break;
		case SOCK_TIME_WAIT:	debug_printf("SOCK_TIME_WAIT\r\n");break;
		case SOCK_LISTEN:		debug_printf("SOCK_LISTEN\r\n");break;
		case SOCK_CLOSE_WAIT:	debug_printf("SOCK_CLOSE_WAIT\r\n");break;
		case SOCK_FIN_WAIT:		debug_printf("SOCK_FIN_WAIT\r\n");break;
		case SOCK_SYNSENT:		debug_printf("SOCK_SYNSENT\r\n");break;
		case SOCK_LAST_ACK:		debug_printf("SOCK_LAST_ACK\r\n");break;
		case SOCK_ESTABLISHE:	debug_printf("SOCK_ESTABLISHE\r\n");break;
		// UDP
		case SOCK_UDP:			debug_printf("SOCK_UDP\r\n");break;
		
		case SOCK_MACRAW:		debug_printf("SOCK_MACRAW\r\n");break;
		default:break;
	}
	
	S_Interrupt irqStat;
	irqStat.Init(soc.IR);
	debug_printf("IR (中断状态):	0x%02X\r\n",soc.IR);
		debug_printf("	CON:		%d\r\n",irqStat.CON);
		debug_printf("	DISCON:	%d\r\n",irqStat.DISCON);
		debug_printf("	RECV:		%d\r\n",irqStat.RECV);
		debug_printf("	TIMEOUT:	%d\r\n",irqStat.TIMEOUT);
		debug_printf("	SEND_OK:	%d\r\n",irqStat.SEND_OK);
		
	debug_printf("DPORT = 0x%02X%02X\r\n",soc.DPORT[1],soc.DPORT[0]);
	
}

bool HardSocket::OnOpen()
{
	if(Index == 0xff)
	{
		debug_printf("Socket 0x%02X 编号不正确，打开失败\r\n",Index);
	}
	if(!Local.Port)
	{
		// 累加端口
		static ushort g_port = 1024;
		if(g_port < 1024) g_port = 1024;
		Local.Port = g_port++;
	}

	//设置分片长度，参考W5500数据手册，该值可以不修改
	if(Protocol == 0x02)
		SocRegWrite2(MSSR, 1472);
	else if(Protocol == 0x01)
		SocRegWrite2(MSSR, 1460);

	// 设置自己的端口号 // 不知道为什么写进去就是错的，少一字节
	SocRegWrite2(PORT, __REV16(Local.Port));	
	//Host->WriteByte(0x04,(Local.Port)>>8,Index,0x01);
	//Host->WriteByte(0x05,(Local.Port),Index,0x01);
	
	
	// 设置端口目的(远程)IP地址
	SocRegWrites(DIPR, Remote.Address.ToArray());
	
	// 设置端口目的(远程)端口号 // 不知道为什么写进去就是错的，少一字节
	SocRegWrite2(DPORT, __REV16(Remote.Port));	
	//Host->WriteByte(0x10,(Remote.Port)>>8,Index,0x01);
	//Host->WriteByte(0x11,(Remote.Port),Index,0x01);

	SocRegWrite(MR, Protocol);	// 设置Socket为UDP模式
	SocRegWrite(CR, OPEN);		// 打开Socket

	Sys.Sleep(5);	//延时5ms

	//如果Socket打开失败
	byte sr = SocRegRead(SR);
	if(Protocol == 0x01 && sr != SOCK_INIT || Protocol == 0x02 && sr != SOCK_UDP)
	{
		debug_printf("Socket %d 打开失败 SR : 0x%02X \r\n",Index,sr);
		//SocRegWrite(CR, CLOSE);	//打开不成功,关闭Socket
		OnClose();
		return false;
	}

	return true;
}

void HardSocket::OnClose()
{
	SocRegWrite(CR, CLOSE);	//打开不成功,关闭Socket
	SocRegWrite(IR, 0xFF);
}

int HardSocket::ReadByteArray(ByteArray& bs)
{
	ushort size = SocRegRead2(RX_RSR);
	// 没接收到数据则返回
	if(size == 0) return 0;	
	
	if(size > 1460) size = 1460;

	ushort offset = SocRegRead2(RX_RD);

	//SPI1_Send_Short(offset);//写16位地址
	//SPI1_Send_Byte(VDM|RWB_READ|(s*0x20+0x18));//写控制字节,N个字节数据长度,读数据,选择端口s的寄存器
	//j = SPI_I2S_ReceiveData(SPI1);

	// 这里需要考虑收到的数据被截成前后两段的情况
	Host->ReadFrame(offset, bs, Index, 0x03);

	// 更新实际物理地址,即下次写待发送数据到发送数据缓冲区的起始地址
	SocRegWrite2(RX_RD, offset);
	
	// 启动接收
	SocRegWrite(CR, RECV);

	// 等待操作完成
	while(SocRegRead(CR));
	
	return size;//返回接收到数据的长度
}

bool HardSocket::WriteByteArray(const ByteArray& bs)
{
	/*//如果是UDP模式,可以在此设置目的主机的IP和端口号
	if((Read_W5500_SOCK_1Byte(s,Sn_MR)&0x0f) != SOCK_UDP)//如果Socket打开失败
	{
		Write_W5500_SOCK_4Byte(s, Sn_DIPR, UDP_DIPR);//设置目的主机IP
		Write_W5500_SOCK_2Byte(s, Sn_DPORTR, UDP_DPORT[0]*256+UDP_DPORT[1]);//设置目的主机端口号
	}*/

	byte st = SocRegRead(SR);
	// 不在UDP  不在TCP连接OK 状态下返回
	if(!(st == SOCK_UDP || st == SOCK_ESTABLISHE))return false;

	bool rs = true;

	// 计算空闲大小
	ushort size = SocRegRead2(TX_FSR);

	// 数据指针
	ushort offset = SocRegRead2(TX_WR);

	//SPI1_Send_Byte(VDM|RWB_WRITE|(s*0x20+0x10));//写控制字节,N个字节数据长度,写数据,选择端口s的寄存器

	// 如果最大地址超过发送缓冲区寄存器的最大地址
	uint len = bs.Length();
	if((offset + len) > size)
	{
		// 只写前面一截数据
		ByteArray bs2(bs.GetBuffer(), size);
		//SocketWrites(offset, bs2);
		Host->WriteFrame(offset, bs2, Index, 0x02);

		//SPI1_Send_Short(0x00);//写16位地址
		//SPI1_Send_Byte(VDM|RWB_WRITE|(s*0x20+0x10));//写控制字节,N个字节数据长度,写数据,选择端口s的寄存器

		// 后面的数据
		bs2.Set(bs.GetBuffer() + size, len - size);
		Host->WriteFrame(offset, bs2, Index, 0x02);

		offset = len - size;
	}
	else
	{
		Host->WriteFrame(offset, bs, Index ,0x02);

		offset += len;
	}

	// 更新实际物理地址,即下次写待发送数据到发送数据缓冲区的起始地址
	SocRegWrite2(TX_WR, offset);

	// 启动发送
	SocRegWrite(CR, SEND);

	// 等待操作完成
	while(SocRegRead(CR));

	S_Interrupt ir;
	while(true)
	{
		Sys.Sleep(60);
		ir.Init(SocRegRead(IR));
		
#ifdef DEBUG		
		debug_printf("IR (中断状态):	0x%02X\r\n",ir.ToByte());
			debug_printf("	CON:		%d\r\n",ir.CON);
			debug_printf("	DISCON:	%d\r\n",ir.DISCON);
			debug_printf("	RECV:		%d\r\n",ir.RECV);
			debug_printf("	TIMEOUT:	%d\r\n",ir.TIMEOUT);
			debug_printf("	SEND_OK:	%d\r\n",ir.SEND_OK);
#endif	
		if(ir.SEND_OK) break;
		byte st = SocRegRead(SR);
		
#ifdef DEBUG
			switch(st)
			{
				// 公共
				case SOCK_CLOSED:		debug_printf("SOCK_CLOSED\r\n");break;
				case SOCK_CLOSING:		debug_printf("SOCK_CLOSING\r\n");break;
				case SOCK_SYNRECV:		debug_printf("SOCK_SYNRECV\r\n");break;
				// TCP
				case SOCK_INIT:			debug_printf("SOCK_INIT\r\n");break;
				case SOCK_TIME_WAIT:	debug_printf("SOCK_TIME_WAIT\r\n");break;
				case SOCK_LISTEN:		debug_printf("SOCK_LISTEN\r\n");break;
				case SOCK_CLOSE_WAIT:	debug_printf("SOCK_CLOSE_WAIT\r\n");break;
				case SOCK_FIN_WAIT:		debug_printf("SOCK_FIN_WAIT\r\n");break;
				case SOCK_SYNSENT:		debug_printf("SOCK_SYNSENT\r\n");break;
				case SOCK_LAST_ACK:		debug_printf("SOCK_LAST_ACK\r\n");break;
				case SOCK_ESTABLISHE:	debug_printf("SOCK_ESTABLISHE\r\n");break;
				// UDP
				case SOCK_UDP:			debug_printf("SOCK_UDP\r\n");break;
				
				case SOCK_MACRAW:		debug_printf("SOCK_MACRAW\r\n");break;
				default:break;
			}
#endif

		if(!(st == SOCK_UDP || st == SOCK_ESTABLISHE))
		{
			debug_printf("SEND_OK Problem!!\r\n");
			
			Close();
			return 0;
		}
	}
	SocRegWrite(IR, ir.ToByte());

	return rs;
}

bool HardSocket::OnWrite(const byte* buf, uint len)
{
	ByteArray bs(buf,len);
	return WriteByteArray(bs);
}

uint HardSocket::OnRead(byte* buf, uint len)
{
	ByteArray bs(buf,len);
	return ReadByteArray(bs);
}

bool HardSocket::IRQ_Process()
{
	// 启动异步接收
	if(_tidRecv) 
	{
		// 激活异步线程
		Sys.SetTask(_tidRecv, true, 0);
		return true;
	}
	return false;
}

void HardSocket::Recovery()
{
	Close();
	// 直接重新打开好了
	Open();
}

void HardSocket::ReceiveTask(void* param)
{
	assert_ptr(param);

	HardSocket* socket = (HardSocket*)param;
	socket->OnIRQ();
}

bool TcpClient::OnOpen()
{
	//SocketWrites(DIPR, Remote.Address.ToArray());

	if(!HardSocket::Open()) return false;

	SocRegWrite(CR, CONNECT);

	// 等待操作完成
	while(SocRegRead(CR));

	while(SocRegRead(SR) != SOCK_SYNSENT)
	{
		if(SocRegRead(SR) == SOCK_ESTABLISHE) return true;

		S_Interrupt ir;
		ir.Init(SocRegRead(IR));
		if(ir.TIMEOUT)
		{
			// 清除超时中断
			SocRegWrite(IR, ir.TIMEOUT);
			return false;
		}
	}

	return true;
}

void TcpClient::OnClose()
{
	SocRegWrite(CR, DISCON);

	HardSocket::Close();
}

bool TcpClient::Listen()
{
	if(!HardSocket::Open()) return false;

	SocRegWrite(CR, LISTEN);	//设置Socket为侦听模式
	Sys.Sleep(5);	//延时5ms

	//如果socket设置失败
	if(SocRegRead(SR) != SOCK_LISTEN)
	{
		SocRegWrite(CR, CLOSE);	//关闭Socket
		return false;
	}

	return true;
}
	// 恢复配置，还要维护连接问题
void TcpClient::Recovery(){}
void TcpClient::OnIRQ(){}

void UdpClient::OnIRQ()
{
	if(Opened)
	{
		// 收到数据
		ByteArray bs;
		int size = ReadByteArray(bs);
		OnReceive(bs.GetBuffer(),size);
		//其他处理..
	}
}

void HardSocket::Register(TransportHandler handler, void* param)
{
	ITransport::Register(handler, param);

	// 如果有注册事件，则启用接收任务
	if(handler)
	{
		if(!_tidRecv)
		{
			// 事件型，只调用一次型
			_tidRecv = Sys.AddTask(ReceiveTask, this, -1, -1, "W5500接收");
			// 关闭任务，等打开以后再开
			if(!Opened) Sys.SetTask(_tidRecv, false);
		}
	}
	else
	{
		if(_tidRecv) Sys.RemoveTask(_tidRecv);
		_tidRecv = 0;
	}
}

