#ifndef __Enc28j60_H__
#define __Enc28j60_H__

#include "Sys.h"
#include "Spi.h"

// Enc28j60类
class Enc28j60
{
private:
    Spi* _spi;
    OutputPort* _ce;

    uint NextPacketPtr;

public:
    byte Bank;

    Enc28j60(Spi* spi, Pin ce = P0, Pin irq = P0);
    virtual ~Enc28j60()
    {
        //if(_spi) delete _spi;
        _spi = NULL;
        
        if(_ce) delete _ce;
        _ce = NULL;
    }

    byte ReadOp(byte op, byte addr);
    void WriteOp(byte op, byte addr, byte data);
    void ReadBuffer(byte* buf, uint len);
    void WriteBuffer(byte* buf, uint len);
    void SetBank(byte addr);
    byte Read(byte addr);
    void Write(byte addr, byte data);
    uint PhyRead(byte addr);
    void PhyWrite(byte addr, uint data);
    void ClockOut(byte clock);
	bool Linked();

    void Init(string mac);
    byte GetRevision();
    void PacketSend(byte* packet, uint len);
    uint PacketReceive(byte* packet, uint maxlen);
};

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

#endif
