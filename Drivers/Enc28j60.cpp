#include "Time.h"
#include "Enc28j60.h"

#define ENC_DEBUG 0
#define NET_DEBUG 0

// ENC28J60 控制寄存器
// 控制寄存器是地址、Bank和Ethernet/MAC/PHY 的组合地址
// - Register address         (bits 0-4)
// - Bank number              (bits 5-6)
// - MAC/PHY indicator        (bit 7)

#define ADDR_MASK        0x1F
#define BANK_MASK        0x60
#define SPRD_MASK        0x80
// All-bank registers
#define EIE              0x1B
#define EIR              0x1C
#define ESTAT            0x1D
#define ECON2            0x1E
#define ECON1            0x1F

// Bank
#define BANK0			0x00
#define BANK1			0x20
#define BANK2			0x40
#define BANK3			0x60

// Bank 0 registers
#define ERDPTL           (0x00|BANK0)
#define ERDPTH           (0x01|BANK0)
#define EWRPTL           (0x02|BANK0)
#define EWRPTH           (0x03|BANK0)
#define ETXSTL           (0x04|BANK0)
#define ETXSTH           (0x05|BANK0)
#define ETXNDL           (0x06|BANK0)
#define ETXNDH           (0x07|BANK0)
#define ERXSTL           (0x08|BANK0)
#define ERXSTH           (0x09|BANK0)
#define ERXNDL           (0x0A|BANK0)
#define ERXNDH           (0x0B|BANK0)
#define ERXRDPTL         (0x0C|BANK0)
#define ERXRDPTH         (0x0D|BANK0)
// ERXWRPTH:ERXWRPTL 寄存器定义硬件向FIFO 中的哪个位置写入其接收到的字节。
// 指针是只读的，在成功接收到一个数据包后，硬件会自动更新指针。 指针可用于判断FIFO内剩余空间的大小。
#define ERXWRPTL         (0x0E|BANK0)
#define ERXWRPTH         (0x0F|BANK0)
#define EDMASTL          (0x10|BANK0)
#define EDMASTH          (0x11|BANK0)
#define EDMANDL          (0x12|BANK0)
#define EDMANDH          (0x13|BANK0)
#define EDMADSTL         (0x14|BANK0)
#define EDMADSTH         (0x15|BANK0)
#define EDMACSL          (0x16|BANK0)
#define EDMACSH          (0x17|BANK0)
// Bank 1 registers
#define EHT0             (0x00|BANK1)
#define EHT1             (0x01|BANK1)
#define EHT2             (0x02|BANK1)
#define EHT3             (0x03|BANK1)
#define EHT4             (0x04|BANK1)
#define EHT5             (0x05|BANK1)
#define EHT6             (0x06|BANK1)
#define EHT7             (0x07|BANK1)
#define EPMM0            (0x08|BANK1)
#define EPMM1            (0x09|BANK1)
#define EPMM2            (0x0A|BANK1)
#define EPMM3            (0x0B|BANK1)
#define EPMM4            (0x0C|BANK1)
#define EPMM5            (0x0D|BANK1)
#define EPMM6            (0x0E|BANK1)
#define EPMM7            (0x0F|BANK1)
#define EPMCSL           (0x10|BANK1)
#define EPMCSH           (0x11|BANK1)
#define EPMOL            (0x14|BANK1)
#define EPMOH            (0x15|BANK1)
#define EWOLIE           (0x16|BANK1)
#define EWOLIR           (0x17|BANK1)
#define ERXFCON          (0x18|BANK1)
#define EPKTCNT          (0x19|BANK1)
// Bank 2 registers
#define MACON1           (0x00|BANK2|0x80)
#define MACON2           (0x01|BANK2|0x80)
#define MACON3           (0x02|BANK2|0x80)
#define MACON4           (0x03|BANK2|0x80)
#define MABBIPG          (0x04|BANK2|0x80)
#define MAIPGL           (0x06|BANK2|0x80)
#define MAIPGH           (0x07|BANK2|0x80)
#define MACLCON1         (0x08|BANK2|0x80)
#define MACLCON2         (0x09|BANK2|0x80)
#define MAMXFLL          (0x0A|BANK2|0x80)
#define MAMXFLH          (0x0B|BANK2|0x80)
#define MAPHSUP          (0x0D|BANK2|0x80)
#define MICON            (0x11|BANK2|0x80)
#define MICMD            (0x12|BANK2|0x80)
#define MIREGADR         (0x14|BANK2|0x80)
#define MIWRL            (0x16|BANK2|0x80)
#define MIWRH            (0x17|BANK2|0x80)
#define MIRDL            (0x18|BANK2|0x80)
#define MIRDH            (0x19|BANK2|0x80)
// Bank 3 registers
#define MAADR1           (0x00|BANK3|0x80)
#define MAADR0           (0x01|BANK3|0x80)
#define MAADR3           (0x02|BANK3|0x80)
#define MAADR2           (0x03|BANK3|0x80)
#define MAADR5           (0x04|BANK3|0x80)
#define MAADR4           (0x05|BANK3|0x80)
#define EBSTSD           (0x06|BANK3)
#define EBSTCON          (0x07|BANK3)
#define EBSTCSL          (0x08|BANK3)
#define EBSTCSH          (0x09|BANK3)
#define MISTAT           (0x0A|BANK3|0x80)
#define EREVID           (0x12|BANK3)
#define ECOCON           (0x15|BANK3)
#define EFLOCON          (0x17|BANK3)
#define EPAUSL           (0x18|BANK3)
#define EPAUSH           (0x19|BANK3)
// PHY registers
#define PHCON1           0x00
#define PHSTAT1          0x01
#define PHHID1           0x02
#define PHHID2           0x03
#define PHCON2           0x10
#define PHSTAT2          0x11
#define PHIE             0x12
#define PHIR             0x13
#define PHLCON           0x14

// ENC28J60 ERXFCON Register Bit Definitions
#define ERXFCON_UCEN     0x80
#define ERXFCON_ANDOR    0x40
#define ERXFCON_CRCEN    0x20
#define ERXFCON_PMEN     0x10
#define ERXFCON_MPEN     0x08
#define ERXFCON_HTEN     0x04
#define ERXFCON_MCEN     0x02
#define ERXFCON_BCEN     0x01
// ENC28J60 EIE Register Bit Definitions
#define EIE_INTIE        0x80
#define EIE_PKTIE        0x40
#define EIE_DMAIE        0x20
#define EIE_LINKIE       0x10
#define EIE_TXIE         0x08
#define EIE_WOLIE        0x04
#define EIE_TXERIE       0x02
#define EIE_RXERIE       0x01
// ENC28J60 EIR Register Bit Definitions
#define EIR_PKTIF        0x40
#define EIR_DMAIF        0x20
#define EIR_LINKIF       0x10
#define EIR_TXIF         0x08
#define EIR_WOLIF        0x04
#define EIR_TXERIF       0x02
#define EIR_RXERIF       0x01
// ENC28J60 ESTAT Register Bit Definitions
#define ESTAT_INT        0x80
#define ESTAT_LATECOL    0x10
#define ESTAT_RXBUSY     0x04
#define ESTAT_TXABRT     0x02
#define ESTAT_CLKRDY     0x01
// ENC28J60 ECON2 Register Bit Definitions
#define ECON2_AUTOINC    0x80
#define ECON2_PKTDEC     0x40
#define ECON2_PWRSV      0x20
#define ECON2_VRPS       0x08
// ENC28J60 ECON1 Register Bit Definitions
#define ECON1_TXRST      0x80
#define ECON1_RXRST      0x40
#define ECON1_DMAST      0x20
#define ECON1_CSUMEN     0x10
#define ECON1_TXRTS      0x08
#define ECON1_RXEN       0x04
#define ECON1_BSEL1      0x02
#define ECON1_BSEL0      0x01
// ENC28J60 MACON1 Register Bit Definitions
#define MACON1_LOOPBK    0x10
#define MACON1_TXPAUS    0x08
#define MACON1_RXPAUS    0x04
#define MACON1_PASSALL   0x02
#define MACON1_MARXEN    0x01
// ENC28J60 MACON2 Register Bit Definitions
#define MACON2_MARST     0x80
#define MACON2_RNDRST    0x40
#define MACON2_MARXRST   0x08
#define MACON2_RFUNRST   0x04
#define MACON2_MATXRST   0x02
#define MACON2_TFUNRST   0x01
// ENC28J60 MACON3 Register Bit Definitions
#define MACON3_PADCFG2   0x80
#define MACON3_PADCFG1   0x40
#define MACON3_PADCFG0   0x20
#define MACON3_TXCRCEN   0x10
#define MACON3_PHDRLEN   0x08
#define MACON3_HFRMLEN   0x04
#define MACON3_FRMLNEN   0x02
#define MACON3_FULDPX    0x01
// ENC28J60 MICMD Register Bit Definitions
#define MICMD_MIISCAN    0x02
#define MICMD_MIIRD      0x01
// ENC28J60 MISTAT Register Bit Definitions
#define MISTAT_NVALID    0x04
#define MISTAT_SCAN      0x02
#define MISTAT_BUSY      0x01
// ENC28J60 PHY PHCON1 Register Bit Definitions
#define PHCON1_PRST      0x8000
#define PHCON1_PLOOPBK   0x4000
#define PHCON1_PPWRSV    0x0800
#define PHCON1_PDPXMD    0x0100
// ENC28J60 PHY PHSTAT1 Register Bit Definitions
#define PHSTAT1_PFDPX    0x1000
#define PHSTAT1_PHDPX    0x0800
#define PHSTAT1_LLSTAT   0x0004
#define PHSTAT1_JBSTAT   0x0002
// ENC28J60 PHY PHCON2 Register Bit Definitions
#define PHCON2_FRCLINK   0x4000
#define PHCON2_TXDIS     0x2000
#define PHCON2_JABBER    0x0400
#define PHCON2_HDLDIS    0x0100

// ENC28J60 Packet Control Byte Bit Definitions
#define PKTCTRL_PHUGEEN  0x08
#define PKTCTRL_PPADEN   0x04
#define PKTCTRL_PCRCEN   0x02
#define PKTCTRL_POVERRIDE 0x01

// SPI operation codes
#define ENC28J60_READ_CTRL_REG       0x00
#define ENC28J60_READ_BUF_MEM        0x3A
#define ENC28J60_WRITE_CTRL_REG      0x40
#define ENC28J60_WRITE_BUF_MEM       0x7A
#define ENC28J60_BIT_FIELD_SET       0x80
#define ENC28J60_BIT_FIELD_CLR       0xA0
#define ENC28J60_SOFT_RESET          0xFF

// The RXSTART_INIT should be zero. See Rev. B4 Silicon Errata
// buffer boundaries applied to internal 8K ram
// the entire available packet buffer space is allocated
//
// start with recbuf at 0/
#define RXSTART_INIT     0x0
// receive buffer end
#define RXSTOP_INIT      (0x1FFF-0x0600-1)
// start TX buffer at 0x1FFF-0x0600, pace for one full ethernet frame (~1500 bytes)
#define TXSTART_INIT     (0x1FFF-0x0600)
// stp TX buffer at end of mem
#define TXSTOP_INIT      0x1FFF
//
// 控制器将接受的最大帧长度
#define        MAX_FRAMELEN        1500        // (note: maximum ethernet frame length would be 1518)
//#define MAX_FRAMELEN     600

typedef union _WORD_VAL
{
    ushort Val;
    byte v[2];
    struct
    {
        byte LB;
        byte HB;
    } byte;
    struct
    {
        unsigned char b0:1;
        unsigned char b1:1;
        unsigned char b2:1;
        unsigned char b3:1;
        unsigned char b4:1;
        unsigned char b5:1;
        unsigned char b6:1;
        unsigned char b7:1;
        unsigned char b8:1;
        unsigned char b9:1;
        unsigned char b10:1;
        unsigned char b11:1;
        unsigned char b12:1;
        unsigned char b13:1;
        unsigned char b14:1;
        unsigned char b15:1;
    } bits;
} WORD_VAL, WORD_BITS;

/*
63-52 零0
51 发送VLAN 标记帧帧的长度/ 类型字段包含VLAN 协议标识符8100h。
50 应用背压流控已应用载波侦听式背压流控
49 发送暂停控制帧发送的帧是带有有效暂停操作码的控制帧。
48 发送控制帧发送的帧是控制帧。
47-32 线上发送的总字节数当前数据包发送在线上的总字节数，包括所有冲突的字节。
31 发送欠载保留。 该位始终为0。
30 发送特大帧帧字节数超过MAMXFL。
29 发送延时冲突冲突发生在冲突窗口（MACLCON2）外。
28 发送过度冲突冲突数超出最大重发数（MACLCON1）后中止数据包。
27 发送过度延期数据包延期大于24287 比特时间（2.4287ms）。
26 发送数据包延期数据包延期至少一次的时间但少于过度延期。
25 发送广播数据包的目标地址是广播地址。
24 发送组播数据包的目标地址是组播地址。
23 发送完成数据包发送完成。
22 发送长度超出范围表示帧类型/ 长度字段大于1500 字节（类型字段）。
21 发送长度校验错误表示数据包中帧长度字段的值与实际的数据字节长度不匹配，它不是类
型字段。 MACON3.FRMLNEN 位必须置1 以便捕获该错误。
20 发送CRC 错误数据包中附加的CRC 与内部生成的CRC 不匹配。
19-16 发送冲突数在尝试发送当前数据包的过程中遇到冲突的次数。 适用于成功发送的数
据包，因此不会是可能发生冲突的最大次数16 次。
15-0 发送字节数帧中的总字节数，不包括冲突字节。
*/
typedef union {
	byte v[7];
	struct {
		ushort	 		ByteCount;
		unsigned char	CollisionCount:4;
		unsigned char	CRCError:1;
		unsigned char	LengthCheckError:1;
		unsigned char	LengthOutOfRange:1;
		unsigned char	Done:1;
		unsigned char	Multicast:1;
		unsigned char	Broadcast:1;
		unsigned char	PacketDefer:1;
		unsigned char	ExcessiveDefer:1;
		unsigned char	MaximumCollisions:1;
		unsigned char	LateCollision:1;
		unsigned char	Giant:1;
		unsigned char	Underrun:1;
		ushort 	 		BytesTransmittedOnWire;
		unsigned char	ControlFrame:1;
		unsigned char	PAUSEControlFrame:1;
		unsigned char	BackpressureApplied:1;
		unsigned char	VLANTaggedFrame:1;
		unsigned char	Zeros:4;
	} bits;
} TXSTATUS;

// 重启任务
void ResetTask(void* param);

Enc28j60::Enc28j60()
{
	_spi = NULL;

	LastTime	= Time.Current();
	ResetPeriod	= 6000000;
	_ResetTask	= 0;

	Broadcast	= true;

	Error		= 0;
}

Enc28j60::~Enc28j60()
{
	delete _spi;
	_spi = NULL;

	if(_ResetTask) Sys.RemoveTask(_ResetTask);
}

void Enc28j60::Init(Spi* spi, Pin ce, Pin reset)
{
	_spi = spi;
	if(ce != P0)
	{
		_ce.OpenDrain = false;
		_ce.Set(ce).Config();
	}
	if(reset != P0)
	{
		_reset.OpenDrain = false;
		_reset.Set(reset).Config();
	}
}

byte Enc28j60::ReadOp(byte op, byte addr)
{
    SpiScope sc(_spi);

	// 操作码和地址
    _spi->Write(op | (addr & ADDR_MASK));
    byte dat = _spi->Write(0xFF);
    // 如果是MAC和MII寄存器，第一个读取的字节无效，该信息包含在地址的最高位
    if(addr & 0x80)
    {
        dat = _spi->Write(0xFF);
    }

    return dat;
}

void Enc28j60::WriteOp(byte op, byte addr, byte data)
{
    SpiScope sc(_spi);

	// 操作码和地址
    _spi->Write(op | (addr & ADDR_MASK));
    _spi->Write(data);
}

void Enc28j60::ReadBuffer(byte* buf, uint len)
{
    SpiScope sc(_spi);

	// 发送读取缓冲区命令
    _spi->Write(ENC28J60_READ_BUF_MEM);
    while(len--)
    {
        *buf++ = _spi->Write(0);
    }
    //*buf='\0';
}

void Enc28j60::WriteBuffer(const byte* buf, uint len)
{
    SpiScope sc(_spi);

	// 发送写取缓冲区命令
    _spi->Write(ENC28J60_WRITE_BUF_MEM);
    while(len--)
    {
        _spi->Write(*buf++);
    }
}

// 设定寄存器地址区域
void Enc28j60::SetBank(byte addr)
{
    // 计算本次寄存器地址在存取区域的位置
    if((addr & BANK_MASK) != Bank)
    {
        // 清除ECON1的BSEL1 BSEL0 详见数据手册15页
        WriteOp(ENC28J60_BIT_FIELD_CLR, ECON1, (ECON1_BSEL1 | ECON1_BSEL0));
		// 请注意寄存器地址的宏定义，bit6 bit5代码寄存器存储区域位置
        WriteOp(ENC28J60_BIT_FIELD_SET, ECON1, (addr & BANK_MASK) >> 5);
		// 重新确定当前寄存器存储区域
        Bank = (addr & BANK_MASK);
    }
}

// 读取寄存器值 发送读寄存器命令和地址
byte Enc28j60::ReadReg(byte addr)
{
    SetBank(addr);
    return ReadOp(ENC28J60_READ_CTRL_REG, addr);
}

// 写寄存器值 发送写寄存器命令和地址
void Enc28j60::WriteReg(byte addr, byte data)
{
    SetBank(addr);
    WriteOp(ENC28J60_WRITE_CTRL_REG, addr, data);
}

// 发送ARP请求包到目的地址
ushort Enc28j60::PhyRead(byte addr)
{
	// 设置PHY寄存器地址
	WriteReg(MIREGADR, addr);
	WriteReg(MICMD, MICMD_MIIRD);

	// 循环等待PHY寄存器被MII读取，需要10.24us
	while((ReadReg(MISTAT) & MISTAT_BUSY));

	// 停止读取
	//WriteReg(MICMD, MICMD_MIIRD);
	WriteReg(MICMD, 0x00);	  // 赋值0x00

	// 获得结果并返回
	return (ReadReg(MIRDH) << 8) | ReadReg(MIRDL);
}

bool Enc28j60::PhyWrite(byte addr, ushort data)
{
    // 向MIREGADR写入地址 详见数据手册19页
    WriteReg(MIREGADR, addr);
    // 写入低8位数据
    WriteReg(MIWRL, data);
	// 写入高8位数据
    WriteReg(MIWRH, data >> 8);

	TimeWheel tw(0, 200, 0);
    // 等待 PHY 写完成
    while(ReadReg(MISTAT) & MISTAT_BUSY)
    {
		if(tw.Expired()) return false;
    }
	return true;
}

void Enc28j60::ClockOut(byte clock)
{
#if NET_DEBUG
	debug_printf("ENC28J60::ECOCON\t= 0x%02X => 0x%02X\r\n", ReadReg(ECOCON), clock & 0x7);
#endif
    // setup clkout: 2 is 12.5MHz:
    WriteReg(ECOCON, clock & 0x7);
}

/*void Enc28j60::Init(byte mac[6])
{
	assert_param(mac);

	memcpy(Mac, mac, 6);
}*/

bool Enc28j60::OnOpen()
{
	//assert_param(Mac);

	debug_printf("Enc28j60::Open(%s)\r\n", Mac.ToString().GetBuffer());

	if(!_reset.Empty())
	{
		_reset = false;
		Sys.Sleep(1);
		_reset = true;
		Sys.Sleep(1);
	}
    if(!_ce.Empty())
    {
        _ce = true;
        Sys.Sleep(100);
        _ce = false;
        Sys.Sleep(100);
        _ce = true;
    }

	// 检查并打开Spi
	_spi->Open();

    // 系统软重启
#if NET_DEBUG
	debug_printf("ENC28J60::RESET\t= 0x%02X => 0x%02X\r\n", ReadReg(0), ENC28J60_SOFT_RESET);
#endif
    WriteOp(ENC28J60_SOFT_RESET, 0, ENC28J60_SOFT_RESET);
	//Sys.Sleep(3);

    // 查询 CLKRDY 位判断是否重启完成
    // The CLKRDY does not work. See Rev. B4 Silicon Errata point. Just wait.
    while(!(ReadReg(ESTAT) & ESTAT_CLKRDY));
    // do bank 0 stuff
    // initialize receive buffer
    // 16-bit transfers, must write low byte first
    // 设置接收缓冲区起始地址 该变量用于每次读取缓冲区时保留下一个包的首地址
    NextPacketPtr = RXSTART_INIT;
    // 设置接收缓冲区 起始指针
    WriteReg(ERXSTL, RXSTART_INIT & 0xFF);
    WriteReg(ERXSTH, RXSTART_INIT >> 8);
    // 设置接收缓冲区 读指针
    WriteReg(ERXRDPTL, RXSTART_INIT & 0xFF);
    WriteReg(ERXRDPTH, RXSTART_INIT >> 8);
    // 设置接收缓冲区 结束指针
    WriteReg(ERXNDL, RXSTOP_INIT & 0xFF);
    WriteReg(ERXNDH, RXSTOP_INIT >> 8);
    // 设置发送缓冲区 起始指针
    WriteReg(ETXSTL, TXSTART_INIT & 0xFF);
    WriteReg(ETXSTH, TXSTART_INIT >> 8);
    // 设置发送缓冲区 结束指针
    WriteReg(ETXNDL, TXSTOP_INIT & 0xFF);
    WriteReg(ETXNDH, TXSTOP_INIT >> 8);

    // Bank 1 填充，包过滤
    // 广播包只允许ARP通过，单播包只允许目的地址是我们mac(MAADR)的数据包
    //
    // The pattern to match on is therefore
    // Type     ETH.DST
    // ARP      BROADCAST
    // 06 08 -- ff ff ff ff ff ff -> ip checksum for theses bytes=f7f9
    // in binary these poitions are:11 0000 0011 1111
    // This is hex 303F->EPMM0=0x3f,EPMM1=0x30

#if NET_DEBUG
	debug_printf("ENC28J60::ERXFCON\t= 0x%02X => 0x%02X\r\n", ReadReg(ERXFCON), ERXFCON_UCEN | ERXFCON_CRCEN | ERXFCON_BCEN);
#endif
	if(Broadcast)
		WriteReg(ERXFCON, ERXFCON_UCEN | ERXFCON_CRCEN | ERXFCON_BCEN); // ERXFCON_BCEN 不过滤广播包，实现DHCP
	else
		// 使能单播过滤 使能CRC校验 使能 格式匹配自动过滤
		WriteReg(ERXFCON, ERXFCON_UCEN | ERXFCON_CRCEN);
#if NET_DEBUG
	debug_printf("ENC28J60::EPMM0\t= 0x%02X => 0x%02X\r\n", ReadReg(EPMM0), 0x3f);
	debug_printf("ENC28J60::EPMM1\t= 0x%02X => 0x%02X\r\n", ReadReg(EPMM1), 0x30);
	debug_printf("ENC28J60::EPMCSL\t= 0x%02X => 0x%02X\r\n", ReadReg(EPMCSL), 0xf9);
	debug_printf("ENC28J60::EPMCSH\t= 0x%02X => 0x%02X\r\n", ReadReg(EPMCSH), 0xf7);
#endif
    WriteReg(EPMM0, 0x3f);
    WriteReg(EPMM1, 0x30);
    WriteReg(EPMCSL, 0xf9);
    WriteReg(EPMCSH, 0xf7);

#if NET_DEBUG
	debug_printf("ENC28J60::MACON1\t= 0x%02X => 0x%02X\r\n", ReadReg(MACON1), MACON1_MARXEN | MACON1_TXPAUS | MACON1_RXPAUS);

	byte dat = ReadReg(MACON2);
	debug_printf("ENC28J60::MACON2\t= 0x%02X => 0x%02X", dat, 0x00);

	//debug_printf("ENC28J60::MACON2");
	if(dat & MACON2_MARST)		debug_printf(" MARST");
	if(dat & MACON2_RNDRST)		debug_printf(" RNDRST");
	if(dat & MACON2_MARXRST)	debug_printf(" MARXRST");
	if(dat & MACON2_RFUNRST)	debug_printf(" RFUNRST");
	if(dat & MACON2_MATXRST)	debug_printf(" MATXRST");
	if(dat & MACON2_TFUNRST)	debug_printf(" TFUNRST");
	debug_printf("\r\n");

	debug_printf("ENC28J60::EFLOCON\t= 0x%02X => 0x%02X\r\n", ReadReg(EFLOCON), 0x02);
#endif
    // Bank 2，使能MAC接收 允许MAC发送暂停控制帧 当接收到暂停控制帧时停止发送
    WriteReg(MACON1, MACON1_MARXEN | MACON1_TXPAUS | MACON1_RXPAUS);
    //WriteReg(MACON1, MACON1_MARXEN);
    // MACON2清零，让MAC退出复位状态
    WriteReg(MACON2, 0x00);
	// 在接收缓冲器空间不足时，主控制器应通过向EFLOCON 寄存器写入02h 打开流量控制。 硬件会周期性地发送暂停帧
	WriteReg(EFLOCON, 0x02);

#if NET_DEBUG
	debug_printf("ENC28J60::MACON3\t= 0x%02X => 0x%02X\r\n", ReadReg(MACON3), MACON3_PADCFG0 | MACON3_TXCRCEN | MACON3_FRMLNEN | MACON3_FULDPX);
#endif
    // 启用自动填充到60字节并进行Crc校验 帧长度校验使能 MAC全双工使能
	// 提示 由于ENC28J60不支持802.3的自动协商机制， 所以对端的网络卡需要强制设置为全双工
    WriteOp(ENC28J60_BIT_FIELD_SET, MACON3, MACON3_PADCFG0 | MACON3_TXCRCEN | MACON3_FRMLNEN | MACON3_FULDPX);
    // 配置非背对背包之间的间隔
    WriteReg(MAIPGL, 0x12);
    WriteReg(MAIPGH, 0x0C);
    // 配置背对背包之间的间隔
    WriteReg(MABBIPG, 0x15);   // 全双工15，半双工12
    // 设置控制器将接收的最大包大小，不要发送大于该大小的包
    WriteReg(MAMXFLL, MAX_FRAMELEN & 0xFF);
    WriteReg(MAMXFLH, MAX_FRAMELEN >> 8);

	// Bank 3 填充
    // write MAC addr
    // NOTE: MAC addr in ENC28J60 is byte-backward
    WriteReg(MAADR5, Mac[0]);
    WriteReg(MAADR4, Mac[1]);
    WriteReg(MAADR3, Mac[2]);
    WriteReg(MAADR2, Mac[3]);
    WriteReg(MAADR1, Mac[4]);
    WriteReg(MAADR0, Mac[5]);

#if NET_DEBUG
	debug_printf("ENC28J60::PHCON1\t= 0x%04X => 0x%04X\r\n", PhyRead(PHCON1), PHCON1_PDPXMD);
	debug_printf("ENC28J60::PHCON2\t= 0x%04X => 0x%04X\r\n", PhyRead(PHCON2), PHCON2_HDLDIS);
	debug_printf("ENC28J60::PHHID1\t= 0x%04X => 0x%04X\r\n", PhyRead(PHHID1), 0x0083);
	debug_printf("ENC28J60::PHHID2\t= 0x%04X => 0x%04X\r\n", PhyRead(PHHID2), 0x1400);
	debug_printf("ENC28J60::PHLCON\t= 0x%04X => 0x%04X\r\n", PhyRead(PHLCON), 0x476);
#endif
	bool flag = true;
    // 配置PHY为全双工  LEDB为拉电流
    if(flag && !PhyWrite(PHCON1, PHCON1_PDPXMD)) flag = false;
    // 阻止发送回路的自动环回
    if(flag && !PhyWrite(PHCON2, PHCON2_HDLDIS)) flag = false;
    // PHY LED 配置,LED用来指示通信的状态
	// 0x476 LEDA 0100 显示链接状态; LEDB 0111 显示发送和接收活动；LFRQ 01 延长LED脉冲至大约73ms
    if(flag && !PhyWrite(PHLCON, 0x476)) flag = false;
    //if(flag && !PhyWrite(PHLCON, 0xE70)) flag = false;
	if(!flag)
	{
		debug_printf("Enc28j60::Init Failed! Can't write Physical, please check Spi!\r\n");
		return false;
	}

    // 切换到bank0
    SetBank(ECON1);
#if NET_DEBUG
	debug_printf("ENC28J60::EIE\t= 0x%02X => 0x%02X\r\n", ReadReg(EIE), EIE_INTIE | EIE_PKTIE | EIE_TXIE | EIE_TXERIE | EIE_RXERIE);
#endif
    // 使能中断 全局中断 接收中断 接收错误中断
    WriteOp(ENC28J60_BIT_FIELD_SET, EIE, EIE_INTIE | EIE_PKTIE | EIE_TXIE | EIE_TXERIE | EIE_RXERIE);
    // 新增加，有些例程里面没有
    //WriteOp(ENC28J60_BIT_FIELD_SET, EIE, EIE_RXERIE | EIE_TXERIE | EIE_INTIE);

#if NET_DEBUG
	debug_printf("ENC28J60::ECON1\t= 0x%02X => 0x%02X\r\n", ReadReg(ECON1), ECON1_RXEN);
#endif
    // 打开包接收
    WriteOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_RXEN);

	byte rev = GetRevision();
	if(rev == 0)
	{
		debug_printf("Enc28j60::Init Failed! Revision=%d\r\n", rev);
		return false;
	}
    // 将enc28j60第三引脚的时钟输出改为：from 6.25MHz to 12.5MHz(本例程该引脚NC,没用到)
    ClockOut(2);

#if NET_DEBUG
	debug_printf("ENC28J60::PHSTAT1\t= 0x%04X\r\n", PhyRead(PHSTAT1));
#endif
	debug_printf("Enc28j60::Inited! Revision=%d\r\n", rev);

	LastTime = Time.Current();

	if(!_ResetTask)
	{
		//debug_printf("Enc28j60::出错检查 ");
		_ResetTask = Sys.AddTask(ResetTask, this, 500000, 500000, "守护2860");
	}

	return true;
}

byte Enc28j60::GetRevision()
{
    // 在EREVID 内也存储了版本信息。 EREVID 是一个只读控制寄存器，包含一个5 位标识符，用来标识器件特定硅片的版本号
    return ReadReg(EREVID);
}

bool Enc28j60::OnWrite(const byte* packet, uint len)
{
	assert_param2(len <= MAX_FRAMELEN, "以太网数据帧超大");

	if(!Linked())
	{
		debug_printf("以太网已断开！\r\n");
		return false;
	}

	// ECON1_TXRTS 发送逻辑正在尝试发送数据包
	int times = 1000;
	while((ReadReg(ECON1) & ECON1_TXRTS) && times-- > 0);
	if(times <= 0)
	{
		debug_printf("Enc28j60::OnWrite 发送失败，设备正忙于发送数据！\r\n");
		return false;
	}

    // 设置写指针为传输数据区域的开头
    WriteReg(EWRPTL, TXSTART_INIT & 0xFF);
    WriteReg(EWRPTH, TXSTART_INIT >> 8);

    // 设置TXND指针为纠正后的给定数据包大小
    WriteReg(ETXNDL, (TXSTART_INIT + len) & 0xFF);
    WriteReg(ETXNDH, (TXSTART_INIT + len) >> 8);

    // 写每个包的控制字节（0x00意味着使用macon3设置）
    WriteOp(ENC28J60_WRITE_BUF_MEM, 0, 0x00);

    // 复制数据包到传输缓冲区
    WriteBuffer(packet, len);

	/*
	仅发送复位通过SPI接口向ECON1寄存器的 TXRST 位写入1可实现仅发送复位。
	如果在 TXRST 位置1时刚好有数据包在发送，硬件会自动将TXRTS 位清零并中止发送。
	该行为只能复位发送逻辑。
	系统复位会自动执行仅发送复位。
	其他寄存器和控制模块（如缓冲管理器和主机接口）将不受仅发送复位的影响。
	主控制器若要恢复正常工作应将TXRST 位清零。
	*/
#if NET_DEBUG
	//debug_printf("ENC28J60::ECON1\t= 0x%02X => 0x%02X\r\n", ReadReg(ECON1), ECON1_TXRTS);
#endif
    // 把传输缓冲区的内容发送到网络
    WriteOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_TXRTS);
	// 等待发送完成
	times = 1000;
	while((ReadReg(ECON1) & ECON1_TXRTS) && times-- > 0);

	/*
	如果数据包发送完成或因错误/ 取消而中止发送，ECON1.TXRTS 位会被清零，一个7 字节的发送状态向量将被写入由ETXND + 1 指向的单元，
	EIR.TXIF 会被置1 并产生中断（如果允许）。
	ETXST 和ETXND 指针将不会被修改。
	要验证数据包是否成功发送，应读取ESTAT.TXABRT 位。
	如果该位置1，主控制器在查询发送状态向量的各个字段外，还应查询ESTAT.LATECOL位，以确定失败的原因。
	*/

#if ENC_DEBUG
	// 根据发送中断寄存器，特殊处理发送冲突延迟导致的失败，实现16次出错重发
    if(GetRevision() == 0x05u || GetRevision() == 0x06u)
	{
		// 如果有 发送错误中断 TXERIF 或者 发送中断 TXIF ，则等待发送结束
		times = 1000;
		while(!(ReadReg(EIR) & (EIR_TXERIF | EIR_TXIF)) && (times-- > 0));

		// 如果有 发送错误中断 TXERIF 或者 超时
		//if((ReadReg(EIR) & EIR_TXERIF) || (times <= 0))
		if((ReadReg(EIR) & EIR_TXERIF) || (ReadReg(ESTAT) & (ESTAT_LATECOL | ESTAT_TXABRT)))
		{
			WORD_VAL ReadPtrSave;
			WORD_VAL TXEnd;
			TXSTATUS TXStatus;
			byte i;

			// 取消上一次发送
			// Cancel the previous transmission if it has become stuck set
			//BFCReg(ECON1, ECON1_TXRTS);
            WriteOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_TXRTS);

			// 保存当前读指针
			// Save the current read pointer (controlled by application)
			ReadPtrSave.v[0] = ReadReg(ERDPTL);
			ReadPtrSave.v[1] = ReadReg(ERDPTH);

			// 获取当前发送状态向量的位置
			// Get the location of the transmit status vector
			TXEnd.v[0] = ReadReg(ETXNDL);
			TXEnd.v[1] = ReadReg(ETXNDH);
			TXEnd.Val++;

			// 读取当前发送状态向量
			// ReadReg the transmit status vector
			WriteReg(ERDPTL, TXEnd.v[0]);
			WriteReg(ERDPTH, TXEnd.v[1]);

			byte* p = (byte*)&TXStatus;
			ReadBuffer((byte*)&TXStatus, sizeof(TXStatus));

#if NET_DEBUG
			MacAddress dest = packet;
			MacAddress src = packet + 6;
			dest.Show();
			debug_printf("<=");
			src.Show();
			debug_printf(" Type=0x%04X Len=%d \r\n", *(ushort*)(packet + 12), len);

			debug_printf("ENC28J60::EIR\t=0x%02X 发送错误中断 发送状态向量 0x%02X 0x%02X", ReadReg(EIR), *(uint*)(p+4), *(uint*)p);
			if(TXStatus.bits.ByteCount)			debug_printf(" 字节数=%d/%d", TXStatus.bits.ByteCount, len);
			if(TXStatus.bits.CollisionCount)	debug_printf(" 冲突数=%d", TXStatus.bits.CollisionCount);
			if(TXStatus.bits.CRCError)			debug_printf(" CRCError");
			if(TXStatus.bits.LengthCheckError)	debug_printf(" LengthCheckError=长度校验错误");
			if(TXStatus.bits.LengthOutOfRange)	debug_printf(" LengthOutOfRange=帧类型/长度字段大于1500字节");
			//if(TXStatus.bits.Done)				debug_printf(" Done发送完成");
			if(TXStatus.bits.Multicast)			debug_printf(" Multicast发送组播");
			if(TXStatus.bits.Broadcast)			debug_printf(" Broadcast发送广播");
			if(TXStatus.bits.PacketDefer)		debug_printf(" PacketDefer发送数据包延期");
			if(TXStatus.bits.ExcessiveDefer)	debug_printf(" ExcessiveDefer发送过度延期");
			if(TXStatus.bits.MaximumCollisions)	debug_printf(" MaximumCollisions过度冲突");
			if(TXStatus.bits.LateCollision)		debug_printf(" LateCollision延时冲突");
			if(TXStatus.bits.Giant)				debug_printf(" Giant特大帧字节数超过MAMXFL");
			if(TXStatus.bits.Underrun)			debug_printf(" Underrun欠载保留");
			if(TXStatus.bits.BytesTransmittedOnWire)debug_printf(" 线上发送的总字节数=%d", TXStatus.bits.BytesTransmittedOnWire);
			if(TXStatus.bits.ControlFrame)		debug_printf(" ControlFrame控制帧");
			if(TXStatus.bits.PAUSEControlFrame)	debug_printf(" PAUSEControlFrame暂停控制帧");
			if(TXStatus.bits.BackpressureApplied)debug_printf(" BackpressureApplied应用背压流控");
			if(TXStatus.bits.VLANTaggedFrame)	debug_printf(" VLANTaggedFrame发送VLAN标记帧");
			debug_printf("\r\n");
#endif

			// 如果发生延迟冲突，则实现重传。这种情况发生于发送时链接信号同时到达
			// Implement retransmission if a late collision occured (this can
			// happen on B5 when certain link pulses arrive at the same time
			// as the transmission)
			for(i = 0; i < 16u; i++)
			{
				if((ReadReg(EIR) & EIR_TXERIF) && TXStatus.bits.LateCollision)
				{
					// 重置发送逻辑
                    WriteOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_TXRTS);
                    WriteOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_TXRTS);
                    WriteOp(ENC28J60_BIT_FIELD_CLR, EIR, EIR_TXERIF | EIR_TXIF);

					// 再次发送数据包
					WriteOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_TXRTS);
					while(!(ReadReg(EIR) & (EIR_TXERIF | EIR_TXIF)));

					// 如果被卡住，取消上一次发送
					// Cancel the previous transmission if it has become stuck set
					WriteOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_TXRTS);

					// 读取发送状态向量
					// ReadReg transmit status vector
					WriteReg(ERDPTL, TXEnd.v[0]);
					WriteReg(ERDPTH, TXEnd.v[1]);
                    ReadBuffer((byte*)&TXStatus, sizeof(TXStatus));
				}
				else
				{
					break;
				}
			}

			// 恢复当前读取指针
			// Restore the current read pointer
			WriteReg(ERDPTL, ReadPtrSave.v[0]);
			WriteReg(ERDPTH, ReadPtrSave.v[1]);
		}
	}
#endif

    // 复位发送逻辑的问题
    if(ReadReg(EIR) & EIR_TXERIF)
    {
		/*
		发送错误中断标志 TXERIF 用于指出是否发生了发送被中止的情况。 以下原因可导致发送被中止：
		1. 发生了MACLCON1 寄存器中最大重发次数（RETMAX）位定义的过度冲突。
		2. 发生了MACLCON2 寄存器中冲突窗口（COLWIN）位定义的延迟冲突。
		3. 在发送完64 字节后，发生了冲突（ESTAT.LATECOL位置1）。
		4. 由于介质被持续占用的时间过长（已达到延迟时限2.4287 ms），无法进行发送。MACON4.DEFER 位被清零。
		5. 在没有将MACON3.HFRMEN位或每个数据包的POVERRIDE 和PHUGEEN 位置1 的情况下，试图发送长度大于由MAMXFL 寄存器定义的最大长度的数据包。
		在全双工模式下，只有第5 种情况会产生该中断。
		*/
#if NET_DEBUG
		debug_printf("ENC28J60::EIR_TXERIF 发送错误中断\r\n");
		ShowStatus();
#endif
		SetBank(ECON1);
        //WriteOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_TXRTS);
		// 参考手册11.3，仅发送复位
		/*
		通过SPI 接口向ECON1 寄存器的TXRST 位写入1 可实现仅发送复位。
		如果在TXRST 位置1 时刚好有数据包在发送，硬件会自动将TXRTS 位清零并中止发送。
		该行为只能复位发送逻辑。 系统复位会自动执行仅发送复位。
		其他寄存器和控制模块（如缓冲管理器和主机接口）将不受仅发送复位的影响。
		主控制器若要恢复正常工作应将TXRST 位清零。
		*/
        WriteOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_TXRST);

		// 发送数据失败
		return false;
    }

	// 检测是否发送成功
	if(ReadReg(ESTAT) & ESTAT_TXABRT)
	{
		debug_printf("ENC28J60::ESTAT_TXABRT 发送中止错误\r\n");
		ShowStatus();

		SetBank(ECON1);
        WriteOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_TXRST);

		return false;
	}

	return true;
}

// 从网络接收缓冲区获取一个数据包，该包开头是以太网头
// packet，该包应该存储到的缓冲区；maxlen，可接受的最大数据长度
uint Enc28j60::OnRead(byte* packet, uint maxlen)
{
    uint rxstat;
    uint len;

	// 检测并打开包接收
	if(!(ReadReg(ECON1) & ECON1_RXEN)) WriteOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_RXEN);

    // 检测缓冲区是否收到一个数据包
	// PKTIF： 接收数据包待处理中断标志位
	// 接收错误中断标志（RXERIF）用于指出接收缓冲器溢出的情况。 也就是说，此中断表明接收缓冲器中的数据包太多，再接收的话将造成EPKTCNT 寄存器溢出。
    if(!(ReadReg(EIR) & EIR_PKTIF))
	{
		// The above does not work. See Rev. B4 Silicon Errata point 6.
		// 通过查看EPKTCNT寄存器再次检查是否收到包
		// EPKTCNT为0表示没有包接收/或包已被处理
		if(ReadReg(EPKTCNT) == 0) return 0;
	}

	// 收到的以太网数据包长度
    if(ReadReg(EPKTCNT) == 0) return 0;

    // 配置接收缓冲器读指针指向地址
    WriteReg(ERDPTL, (NextPacketPtr));
    WriteReg(ERDPTH, (NextPacketPtr) >> 8);

    // 下一个数据包的读指针
    NextPacketPtr  = ReadOp(ENC28J60_READ_BUF_MEM, 0);
    NextPacketPtr |= ReadOp(ENC28J60_READ_BUF_MEM, 0) << 8;

    // 读数据包字节长度 (see datasheet page 43)
    len  = ReadOp(ENC28J60_READ_BUF_MEM, 0);
    len |= ReadOp(ENC28J60_READ_BUF_MEM, 0) << 8;

	// 去除CRC校验部分
    len -= 4;
    // 读接收数据包的状态 (see datasheet page 43)
    rxstat  = ReadOp(ENC28J60_READ_BUF_MEM, 0);
    rxstat |= ReadOp(ENC28J60_READ_BUF_MEM, 0) << 8;
    // 限制获取的长度。有些例程这里不用减一
    if (len > maxlen - 1) len = maxlen - 1;

    // 检查CRC和符号错误
    // ERXFCON.CRCEN是默认设置。通常我们不需要检查
    if ((rxstat & 0x80) == 0)
    {
        // 无效的
        len = 0;
    }
    else
    {
        // 从缓冲区中将数据包复制到packet中
        ReadBuffer(packet, len);
    }
    // 移动接收缓冲区 读指针
    WriteReg(ERXRDPTL, (NextPacketPtr));
    WriteReg(ERXRDPTH, (NextPacketPtr) >> 8);

#if NET_DEBUG
	//debug_printf("ENC28J60::ECON2\t= 0x%02X => 0x%02X\r\n", ReadReg(ECON2), ECON2_PKTDEC);
#endif
    // 数据包个数递减位EPKTCNT减1
    WriteOp(ENC28J60_BIT_FIELD_SET, ECON2, ECON2_PKTDEC);

	// 最后收到数据包的时间
	LastTime = Time.Current();

    return len;
}

// 返回MAC连接状态
bool Enc28j60::Linked()
{
	return PhyRead(PHSTAT1) & PHSTAT1_LLSTAT;
}

// 显示寄存器状态
void Enc28j60::ShowStatus()
{
#if NET_DEBUG
	debug_printf("\r\n寄存器状态：\r\n");

	byte dat = ReadReg(MACON1);
	if(dat != 0x0D) debug_printf("ENC28J60::MACON1\t= 0x%02X\r\n", dat);
	dat = ReadReg(MACON2);
	if(dat != 0x00)
	{
		debug_printf("ENC28J60::MACON2\t= 0x%02X", dat);
		if(dat & MACON2_MARST)		debug_printf(" MARST");
		if(dat & MACON2_RNDRST)		debug_printf(" RNDRST");
		if(dat & MACON2_MARXRST)	debug_printf(" MARXRST");
		if(dat & MACON2_RFUNRST)	debug_printf(" RFUNRST");
		if(dat & MACON2_MATXRST)	debug_printf(" MATXRST");
		if(dat & MACON2_TFUNRST)	debug_printf(" TFUNRST");
		debug_printf("\r\n");
	}
	dat = ReadReg(MACON3);
	if(dat != 0x33) debug_printf("ENC28J60::MACON3\t= 0x%02X\r\n", dat);

	//SetBank(ECON1);
	dat = ReadReg(EIE);
	if(dat != 0xCB) debug_printf("ENC28J60::EIE\t= 0x%02X\r\n", dat);

	dat = ReadReg(EIR);
	if(dat != 0x00 && dat != EIR_TXIF)
	{
		debug_printf("ENC28J60::EIR\t= 0x%02X", dat);
		if(dat & EIR_PKTIF)		debug_printf(" PKTIF");
		if(dat & EIR_DMAIF)		debug_printf(" DMAIF");
		if(dat & EIR_LINKIF)	debug_printf(" LINKIF");
		if(dat & EIR_TXIF)		debug_printf(" TXIF");
		if(dat & EIR_WOLIF)		debug_printf(" WOLIF");
		if(dat & EIR_TXERIF)	debug_printf(" TXERIF");
		if(dat & EIR_RXERIF)	debug_printf(" RXERIF");
		debug_printf("\r\n");
	}

	dat = ReadReg(ESTAT);
	if(dat != ESTAT_CLKRDY)
	{
		debug_printf("Enc28j60::ESTAT\t=0x%02X", dat);
		if(dat & ESTAT_INT)		debug_printf(" INT");
		if(dat & ESTAT_LATECOL)	debug_printf(" LATECOL");
		if(dat & ESTAT_RXBUSY)		debug_printf(" RXBUSY");
		if(dat & ESTAT_TXABRT)		debug_printf(" TXABRT");
		if(dat & ESTAT_CLKRDY)		debug_printf(" CLKRDY");
		debug_printf("\r\n");
	}

	dat = ReadReg(ECON2);
	if(dat != 0x80) debug_printf("ENC28J60::ECON2\t= 0x%02X\r\n", dat);
	dat = ReadReg(ECON1);
	if(dat != 0x04) debug_printf("ENC28J60::ECON1\t= 0x%02X\r\n", dat);

	ushort pst = PhyRead(PHSTAT1);
	if(pst != 0x1804) debug_printf("ENC28J60::PHSTAT1\t= 0x%04X\r\n", pst);
	pst = PhyRead(PHSTAT2);
	if(pst != 0x0400) debug_printf("ENC28J60::PHSTAT2\t= 0x%04X\r\n", pst);
#endif
}

void Enc28j60::CheckError()
{
	//ShowStatus();

	// 如果已经被重启，重新初始化
	/*byte dat = ReadReg(MACON2);
	if(dat)
	{
		Error++;

		debug_printf("Enc28j60::MACON2\t= 0x%02X，重新初始化。共出错 %d 次 ", dat, Error);
		Opened = false;
		Open();
		return;
	}*/

	ulong ts = Time.Current() - LastTime;
	if(ResetPeriod < ts)
	{
		Error++;

		debug_printf("Enc28j60::超过%d秒没有收到任何数据，重新初始化。共出错 %d 次 ", (int)(ts/1000000), Error);
		Opened = false;
		Open();
	}
}

// 设置是否接收广播数据包
void Enc28j60::SetBroadcast(bool flag)
{
	Broadcast = flag;
	//SetBank(ECON1);
	if(flag)
		WriteReg(ERXFCON, ERXFCON_UCEN | ERXFCON_CRCEN | ERXFCON_BCEN); // ERXFCON_BCEN 不过滤广播包，实现DHCP
	else
		// 使能单播过滤 使能CRC校验 使能 格式匹配自动过滤
		WriteReg(ERXFCON, ERXFCON_UCEN | ERXFCON_CRCEN);
}


void ResetTask(void* param)
{
	Enc28j60* enc = (Enc28j60*)param;
	enc->CheckError();
}
