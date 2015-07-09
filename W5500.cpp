
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
						// XXX01	SocketXXX 寄存器
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

W5500::W5500(Spi* spi, Pin irq, OutputPort* rst)
{
	Init();
	Init(spi, irq,rst);
}

W5500::~W5500() { }
// 复位（软硬兼施）
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
// 软件复位
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
// 初始化
void W5500::Init()
{
	_Lock = 0;
	_spi = NULL;
	nRest = NULL;
	memset(&General_reg,0,sizeof(General_reg));
	memset(&_socket,NULL,sizeof(_socket));
	PhaseOM = 0x00;
	RX_FREE_SIZE = 0x16;
	TX_FREE_SIZE = 0x16;
}
// 初始化
void W5500::Init(Spi* spi, Pin irq, OutputPort* rst)
{
	if(rst) nRest = rst;
	if(irq != P0)
	{	
		debug_printf("\r\nW5500::Init IRQ = P%c%d\r\n",_PIN_NAME(irq));
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
	
	Frame frame;
	frame.Address 	= 0x0039;
	frame.BSB 		= 0x00;
	ReadFrame(frame,1);
	byte version = frame.Data.Read<byte>();
	debug_printf("Hard Vision: %02X\r\n",version);
	
	//Frame frame;
	frame.Address = 0x0000;
	frame.BSB = 0x00;
	frame.Data.SetPosition(0);
	
	ReadFrame(frame,sizeof(General_reg));
	frame.Data.Read((byte *)&General_reg,0,sizeof(General_reg));
}
// 网卡状态输出
void W5500::StateShow()
{
	debug_printf("\r\nW5500 State:\r\n");
	
	debug_printf("MR (模式): 		0x%02X   ",General_reg.MR);
		T_MR mr;
		mr.Init(General_reg.MR);
		debug_printf("WOL: %d   ",mr.WOL);
		debug_printf("PB: %d   ",mr.PB);
		debug_printf("PPPoE: %d   ",mr.PPPoE);
		debug_printf("FARP: %d   ",mr.FARP);
		debug_printf("\r\n");
	debug_printf("GAR (网关地址): 	%d.%d.%d.%d\r\n",General_reg.GAR[0],General_reg.GAR[1],General_reg.GAR[2],General_reg.GAR[3]);
	debug_printf("SUBR (子网掩码): 	%d.%d.%d.%d\r\n",General_reg.SUBR[0],General_reg.SUBR[1],General_reg.SUBR[2],General_reg.SUBR[3]);
	debug_printf("SHAR (源MAC地址):	%02X-%02X-%02X-%02X-%02X-%02X\r\n",General_reg.SHAR[0],General_reg.SHAR[1],General_reg.SHAR[2],General_reg.SHAR[3],General_reg.SHAR[4],General_reg.SHAR[5]);
	debug_printf("SIPR (源IP地址): 	%d.%d.%d.%d\r\n",General_reg.SIPR[0],General_reg.SIPR[1],General_reg.SIPR[2],General_reg.SIPR[3]);
	debug_printf("INTLEVEL(中断低电平时间): 0x%02X%02X\r\n",General_reg.INTLEVEL[1],General_reg.INTLEVEL[0]);	// 回头计算一下
	debug_printf("IMR (中断屏蔽): 	0x%02X   ",General_reg.IMR);
		T_IR imr;
		imr.Init(General_reg.MR);
		debug_printf("CONFLICT: %d   ",imr.CONFLICT);
		debug_printf("UNREACH: %d   ",imr.UNREACH);
		debug_printf("PPPoE: %d   ",imr.PPPoE);
		debug_printf("MP: %d   ",imr.MP);
		debug_printf("\r\n");
	debug_printf("SIMR (Socket中断屏蔽): 0x%02X\r\n",General_reg.SIMR);
	debug_printf("RTR (重试时间): 	0x%02X%02X\r\n",General_reg.RTR[1],General_reg.RTR[0]);		// 回头计算一下
	debug_printf("RCR (重试计数): 	%d 次\r\n",General_reg.RCR);

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
	phy.Init(General_reg.PHYCFGR);
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

bool W5500::WriteFrame(Frame& frame)
{
	while(_Lock != 0)Sys.Sleep(0);
	_Lock = 1;
	
	SpiScope sc(_spi);
	byte temp = frame.Address>>8;	// 地址高位在前
	_spi->Write(temp);
	temp = frame.Address;
	_spi->Write(temp);
	
	CONFIG_Phase config_temp;		// 配置读写和OM
	config_temp.Init(frame.BSB<<3);
	config_temp.OM = PhaseOM;		// 类内整体
	config_temp.RWB = 1;			// 写
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
	w5500_printf("\r\n");
	
	_Lock = 0;
	return true;
}

bool W5500::ReadFrame(Frame& frame,uint length)
{
	while(_Lock != 0)Sys.Sleep(0);
	_Lock = 1;
	
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
	w5500_printf("\r\n");
	frame.Data.SetPosition(0);
	
	_Lock = 0;
	return true;
}

// 测试PHY状态  返回是否LNK
bool W5500::CheckLnk()
{
	Frame frame;
	frame.Address = 0x002e;
	frame.BSB = 0x00;
	ReadFrame(frame,1);
	
	General_reg.PHYCFGR = frame.Data.Read<byte>();
	
	T_PHYCFGR phy;
	phy.Init(General_reg.PHYCFGR);
	return phy.LNK;
}
// 设置MAC地址
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
// “随机”一个MAC  并设置
void W5500::AutoMac()
{
	MacAddress mac;
	// 随机Mac，前三个字节取自YWS的ASCII，最后3个字节取自后三个ID
	mac[0] = 'P'; mac[1] = 'M'; //mac[2] = 'W'; mac[3] = 'L';
	for(int i=0; i< 4; i++)
		mac[2 + i] = Sys.ID[3 - i];
	// MAC地址首字节奇数表示组地址，这里用偶数
	SetMac(mac);
}
// 获取MAC地址
MacAddress W5500::Mac()
{
	_mac.Value = (*(ulong *)* General_reg.SHAR) & 0xFFFFFFFFFFFFull;
	return _mac;
}
// 设置网关IP
void W5500::SetGateway(IPAddress& ip)
{
	memcpy(General_reg.GAR,&ip.Value,4);
	Frame frame;
	frame.Address = (ushort)((uint)General_reg.GAR - (uint)&General_reg);
	frame.BSB =  0x00;	// 通用寄存器区
	frame.Data.Write<uint>(ip.Value);
	WriteFrame(frame);
}
// 设置默认网关IP
void W5500::DefGateway()
{
	const byte defip_[] = {192, 168, 0, 1};
	IPAddress defip(defip_);
	SetGateway(defip);
}
// 设置子网掩码
void W5500::SetIpMask(IPAddress& mask)
{
	short temp[2];
	temp[1] = __REV16(mask.Value);
	temp[0] = __REV16(mask.Value >> 16);
	
	memcpy(General_reg.SUBR,temp,4);
	Frame frame;
	frame.Address = (ushort)((uint)General_reg.SUBR - (uint)&General_reg);
	frame.BSB =  0x00;	// 通用寄存器区
	frame.Data.Write<uint>(mask.Value);
	WriteFrame(frame);
}
// 设置默认子网掩码
void W5500::DefIpMask()
{
	IPAddress defip;
	defip = 0xFFFFFF00;
	SetIpMask(defip);
}
// 设置自己的IP
void W5500::SetMyIp(IPAddress& ip)
{
	memcpy(General_reg.SIPR,&ip.Value,4);
	Frame frame;
	frame.Address = (ushort)((uint)General_reg.SIPR - (uint)&General_reg);
	frame.BSB =  0x00;	// 通用寄存器区
	frame.Data.Write<uint>(ip.Value);
	WriteFrame(frame);
}
// 设置超时时间
void W5500::SetRetryTime(ushort ms)
{
	// RTR 单位是100us 所以最大6553ms
	if(ms > 6553)
	{
		debug_printf("不支持的时间范围！\r\n");
		return;
	}
	ushort time = ms*10;
	
	memcpy(General_reg.RTR,&time,2);
	Frame frame;
	frame.Address = (ushort)((uint)General_reg.RTR - (uint)&General_reg);
	frame.BSB =  0x00;	// 通用寄存器区
	frame.Data.Write<ushort>(time);
	
	WriteFrame(frame);
}
// 设置重试次数
void W5500::SetRetryCount(byte count)
{
	General_reg.RCR = count;
	Frame frame;
	frame.Address = (ushort)((uint)General_reg.RCR - (uint)&General_reg);
	frame.BSB =  0x00;	// 通用寄存器区
	frame.Data.Write<byte>(count);
	
	WriteFrame(frame);
}
// 开启PING应答
void W5500::OpenPingACK()
{
	T_MR mr;
	mr.Init(General_reg.MR);
	mr.PB = 0;		// 0 是有响应
	General_reg.MR = mr.ToByte();
	
	Frame frame;
	frame.Address = 0x0000;
	frame.BSB =  0x00;	// 通用寄存器区
	frame.Data.Write<byte>(mr.ToByte());
	WriteFrame(frame);
}
// 开启PING应答
void W5500::ClosePingACK()
{
	T_MR mr;
	mr.Init(General_reg.MR);
	mr.PB = 1;		// 0 是有响应
	General_reg.MR = mr.ToByte();
	
	Frame frame;
	frame.Address = 0x0000;
	frame.BSB =  0x00;	// 通用寄存器区
	frame.Data.Write<byte>(mr.ToByte());
	WriteFrame(frame);
}
// 恢复全部状态
void W5500::Recovery()
{
	Frame frame;
	frame.Address = 0x0000;
	frame.BSB = 0x00;
	frame.Data.Write<struct T_GenReg>(General_reg);
	WriteFrame(frame);	
	// PHY   配置后要重启特殊处理 
	frame.Address = (ushort)((uint)General_reg.PHYCFGR - (uint)&General_reg);
	T_PHYCFGR phy;
	phy.Init(General_reg.PHYCFGR);
	phy.RST = 0;	// 0重启
	frame.Data.SetPosition(0);
	frame.Data.Write<byte>(phy.ToByte());
	WriteFrame(frame);
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
// 注册 Socket
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
								// [UCASTB]	 单播阻撒	条件：【UDP 模式】且MULTI=1
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
								// 注意：使用多播模式，需要Sn_DIPR  Sn_DPORT 在通过Sn_CR 打开配置命令打开之前，分别配置组播IP地址及端口号
								// [MFEN] MAC 过滤		条件：【MACRAW 模式】
								// 		0: 关闭 MAC 过滤，收到网络的所有包
								//		1：开启 MAC 过滤，只接收广播包和发给自己的包
	}TSn_MR;
	// Sn_CR 寄存器比较特殊
	// 写入后会自动清零，但命令任然在处理中，为验证命令是否完成，需检查Sn_IR或者SnSR寄存器
	enum ESn_CR
	{
		OPEN		= 0x01,	// open 之后，检测 Sn_SR 的值
							// 	Sn_MR.Protocol		Sn_SR
							// 		【CLOSE】		-		
							// 		【TCP】			【SOCK_INT】0X13
							// 		【UDP】			【SOCK_UDP】0X22
							// 		【MACRAW】		【SOCK_MACRAW】0X02
							
		LISTEN		= 0x02,	// 只在【TCP 模式】下生效	LISTEN 会把 Socket 变成服务端，等待TCP连接
							// LISTEN 把 SOCK_INIT 变为 SOCK_LISTEN
							// 当 SOCK 被连接成功 SOCK_LISTEN 变为 SOCK_ESTABLISHE,同时 Sn_IR.CON =1
							// 		当连接非法断开 Sn_IR.TIMEOUT =1， Sn_SR = SOCK_CLOSED
		
		CONNECT 	= 0x04,	// 只在【TCP 模式】下生效	Socket 作为客户端  对 Sn_DIPR:Sn_DPORT 发起连接 
							// 链接成功时 Sn_SR = SOCK_ESTABLISHE  Sn_IR.CON =1
							// 三种情况意味着请求失败  Sn_SR = SOCK_CLOSED
							// 	1.ARP 发生超时(Sn_IR.TIMEOUT =1)
							//  2.没有收到 SYN/ACK 数据包(Sn_IR.TIMEOUT = 1)
							//  3.收到RST数据包
							
		DISCON		= 0x08,	// 只在【TCP 模式】下生效	FIN/ACK 断开机制断开连接  （不分 客户端，服务端）
							// Sn_SR = SOCK_CLOSED
							// 另外 当断开请求没有收到ACK时 Sn_IR.TIMEOUT =1
							// 使用 CLOSE 命令来代替 DISCON 的话，Sn_SR = SOCK_CLOSED 不再执行 FIN/ACK 断开机制
							// 收到RST数据包 将无条件 Sn_SR = SOCK_CLOSED
							
		CLOSE		= 0x10,	// 关闭Socket   Sn_SR = SOCK_CLOSED
		
		SEND		= 0x20,	// 发送 Socket TX内存中的所有缓冲数据
							// 一般来说，先通常自动ARP获取到MAC才进行传输
							
		SEND_MAC	= 0x21,	// 只在【UDP 模式】下生效
							// 使用Sn_DHAR 中的MAC进行发送（不进行ARP）
							
		SEND_KEEP	= 0x22,	// 只在【UDP 模式】下生效	通过发送1字节在线心跳包检查连接状态
							// 如果对方不能在超时计数期内反馈在线心跳包，这个连接将被关闭并触发中断
							
		RECV		= 0x40,	// 通过使用接收读指针寄存器 Sn_RX_RD 来判定 接收缓存是否完成接收处理
	};	
	//中断寄存器
	typedef struct : ByteStruct
	{
		byte CON:1;			// 连接成功 	Sn_SR = SOCK_ESTABLISHE 此位生效
		byte DISCON:1;		// 连接中断		接收到对方 FIN/ACK 断开机制数据包时 此位生效
		byte RECV:1;		// 无论何时，接收到对方数据包，此位生效
		byte TIMEOUT:1;		// ARP  TCP 超时被触发
		byte SEND_OK:1;		// 发送完成
		byte Reserved:3;
	}TSn_IR;
	// 状态
	enum ESn_SR
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
	
HardwareSocket::HardwareSocket(W5500* phard)
{
	if(!phard)
	{
		_THard = phard;
		Index = _THard->GetSocket();
		if(Index < 8)
			_THard->Register(Index , this);
	}
}

HardwareSocket::~HardwareSocket()
{
}
