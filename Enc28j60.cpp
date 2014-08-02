#include "Enc28j60.h"

Enc28j60::Enc28j60(Spi* spi, Pin ce, Pin irq)
{
    _spi = spi;
    if(ce != P0) _ce = new OutputPort(ce);
}

byte Enc28j60::ReadOp(byte op, byte addr)
{
    _spi->Start();

    byte dat = op | (addr & ADDR_MASK);
    _spi->Write(dat);
    dat = _spi->Write(0xFF);
    // do dummy read if needed (for mac and mii, see datasheet page 29)
    if(addr & 0x80)
    {
        dat = _spi->Write(0xFF);
    }

    _spi->Stop();

    return dat;
}

void Enc28j60::WriteOp(byte op, byte addr, byte data)
{
    _spi->Start();

    // issue write command
    byte dat = op | (addr & ADDR_MASK);
    _spi->Write(dat);
    // write data
    dat = data;
    _spi->Write(dat);

    _spi->Stop();
}

void Enc28j60::ReadBuffer(byte* buf, uint len)
{
    _spi->Start();

    // issue read command
    _spi->Write(ENC28J60_READ_BUF_MEM);
    while(len--)
    {
        *buf++ = _spi->Write(0);
    }
    *buf='\0';

    _spi->Stop();
}

void Enc28j60::WriteBuffer(byte* buf, uint len)
{
    _spi->Start();

    // issue write command
    _spi->Write(ENC28J60_WRITE_BUF_MEM);
    
    while(len--)
    {
        _spi->Write(*buf++);
    }

    _spi->Stop();
}

void Enc28j60::SetBank(byte addr)
{
    // set the bank (if needed)
    if((addr & BANK_MASK) != Bank)
    {
        // set the bank
        WriteOp(ENC28J60_BIT_FIELD_CLR, ECON1, (ECON1_BSEL1 | ECON1_BSEL0));
        WriteOp(ENC28J60_BIT_FIELD_SET, ECON1, (addr & BANK_MASK) >> 5);
        Bank = (addr & BANK_MASK);
    }
}

byte Enc28j60::Read(byte addr)
{
    // set the bank
    SetBank(addr);
    // do the read
    return ReadOp(ENC28J60_READ_CTRL_REG, addr);
}

void Enc28j60::Write(byte addr, byte data)
{
    // set the bank
    SetBank(addr);
    // do the write
    WriteOp(ENC28J60_WRITE_CTRL_REG, addr, data);
}

void Enc28j60::PhyWrite(byte addr, uint data)
{
    // set the PHY register addr
    Write(MIREGADR, addr);
    // write the PHY data
    Write(MIWRL, data);
    Write(MIWRH, data >> 8);
    // wait until the PHY write completes
    while(Read(MISTAT) & MISTAT_BUSY)
    {
        //Del_10us(1);
        //_nop_();
    }
}

void Enc28j60::ClockOut(byte clock)
{
    //setup clkout: 2 is 12.5MHz:
    Write(ECOCON, clock & 0x7);
}

void Enc28j60::Init(string mac)
{
    if(_ce)
    {
        *_ce = true;
        Sys.Sleep(100);
        *_ce = false;
        Sys.Sleep(10000);
        *_ce = true;
    }

    _spi->Stop();

    // 系统软重启
    WriteOp(ENC28J60_SOFT_RESET, 0, ENC28J60_SOFT_RESET);
   
    // check CLKRDY bit to see if reset is complete
    // The CLKRDY does not work. See Rev. B4 Silicon Errata point. Just wait.
    //while(!(Read(ESTAT) & ESTAT_CLKRDY));
    // do bank 0 stuff
    // initialize receive buffer
    // 16-bit transfers, must write low byte first
    // 设置接收缓冲区开始地址
    NextPacketPtr = RXSTART_INIT;
    // Rx开始
    Write(ERXSTL, RXSTART_INIT & 0xFF);	 
    Write(ERXSTH, RXSTART_INIT >> 8);
    // 设置接收指针地址
    Write(ERXRDPTL, RXSTART_INIT & 0xFF);
    Write(ERXRDPTH, RXSTART_INIT >> 8);
    // Rx结束
    Write(ERXNDL, RXSTOP_INIT & 0xFF);
    Write(ERXNDH, RXSTOP_INIT >> 8);
    // TX 开始 1500
    Write(ETXSTL, TXSTART_INIT & 0xFF);
    Write(ETXSTH, TXSTART_INIT >> 8);
    // TX 结束
    Write(ETXNDL, TXSTOP_INIT & 0xFF);
    Write(ETXNDH, TXSTOP_INIT >> 8);
    // do bank 1 stuff, packet filter:
    // For broadcast packets we allow only ARP packtets
    // All other packets should be unicast only for our mac (MAADR)
    //
    // The pattern to match on is therefore
    // Type     ETH.DST
    // ARP      BROADCAST
    // 06 08 -- ff ff ff ff ff ff -> ip checksum for theses bytes=f7f9
    // in binary these poitions are:11 0000 0011 1111
    // This is hex 303F->EPMM0=0x3f,EPMM1=0x30
    
    //Write(ERXFCON, ERXFCON_UCEN|ERXFCON_CRCEN|ERXFCON_PMEN);
    Write(ERXFCON, ERXFCON_UCEN | ERXFCON_CRCEN | ERXFCON_BCEN); ///ERXFCON_BCEN 不过滤广播包
    Write(EPMM0, 0x3f);
    Write(EPMM1, 0x30);
    Write(EPMCSL, 0xf9);
    Write(EPMCSH, 0xf7);
    
    // Bank 2，打开MAC接收
    Write(MACON1, MACON1_MARXEN | MACON1_TXPAUS | MACON1_RXPAUS);
    // bring MAC out of reset 
    Write(MACON2, 0x00);
    // 启用自动填充到60字节并进行Crc校验
    WriteOp(ENC28J60_BIT_FIELD_SET, MACON3, MACON3_PADCFG0 | MACON3_TXCRCEN | MACON3_FRMLNEN | MACON3_FULDPX);
    // set inter-frame gap (non-back-to-back)
    Write(MAIPGL, 0x12);
    Write(MAIPGH, 0x0C);
    // set inter-frame gap (back-to-back)
    Write(MABBIPG, 0x15);   // 有的例程这里是0x12
    // 设置控制器将接收的最大包大小，不要发送大于该大小的包
    Write(MAMXFLL, MAX_FRAMELEN & 0xFF);	
    Write(MAMXFLH, MAX_FRAMELEN >> 8);
    // do bank 3 stuff
    // write MAC addr
    // NOTE: MAC addr in ENC28J60 is byte-backward
    Write(MAADR5, mac[0]);	
    Write(MAADR4, mac[1]);
    Write(MAADR3, mac[2]);
    Write(MAADR2, mac[3]);
    Write(MAADR1, mac[4]);
    Write(MAADR0, mac[5]);

    // 配置PHY为全双工  LEDB为拉电流
    PhyWrite(PHCON1, PHCON1_PDPXMD);    
    // no loopback of transmitted frames
    PhyWrite(PHCON2, PHCON2_HDLDIS);
    // switch to bank 0    
    SetBank(ECON1);
    // enable interrutps
    WriteOp(ENC28J60_BIT_FIELD_SET, EIE, EIE_INTIE | EIE_PKTIE);
    // 新增加，有些例程里面没有
    WriteOp(ENC28J60_BIT_FIELD_SET, EIE, EIE_RXERIE | EIE_TXERIE | EIE_INTIE);
    // enable packet reception
    WriteOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_RXEN);
}

byte Enc28j60::GetRevision()
{
    //在EREVID 内也存储了版本信息。 EREVID 是一个只读控
    //制寄存器，包含一个5 位标识符，用来标识器件特定硅片
    //的版本号
    return Read(EREVID);
}

void Enc28j60::PacketSend(byte* packet, uint len)
{
    // 设置写指针为传输数据区域的开头
    Write(EWRPTL, TXSTART_INIT & 0xFF);
    Write(EWRPTH, TXSTART_INIT >> 8);
    
    // 设置TXND指针为纠正后的给定数据包大小
    Write(ETXNDL, (TXSTART_INIT + len) & 0xFF);
    Write(ETXNDH, (TXSTART_INIT + len) >> 8);
    
    // 写每个包的控制字节（0x00意味着使用macon3设置）
    WriteOp(ENC28J60_WRITE_BUF_MEM, 0, 0x00);
    
    // 复制数据包到传输缓冲区
    WriteBuffer(packet, len);
    
    // 把传输缓冲区的内容发送到网络
    WriteOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_TXRTS);
    
    // Reset the transmit logic problem. See Rev. B4 Silicon Errata point 12.
    if( (Read(EIR) & EIR_TXERIF) )
    {
        WriteOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_TXRTS);
    }
}

// 从网络接收缓冲区获取一个数据包，该包开头是以太网头
// packet，该包应该存储到的缓冲区；maxlen，可接受的最大数据长度
uint Enc28j60::PacketReceive(byte* packet, uint maxlen)
{
    unsigned int rxstat;
    unsigned int len;
    
    // check if a packet has been received and buffered
    //if( !(Read(EIR) & EIR_PKTIF) ){
    // The above does not work. See Rev. B4 Silicon Errata point 6.
    if( Read(EPKTCNT) ==0 )  // 收到的以太网数据包长度
    {
        return 0;
    }
    
    // Set the read pointer to the start of the received packet		 缓冲器读指针
    Write(ERDPTL, (NextPacketPtr));
    Write(ERDPTH, (NextPacketPtr) >> 8);
    
    // read the next packet pointer
    NextPacketPtr  = ReadOp(ENC28J60_READ_BUF_MEM, 0);
    NextPacketPtr |= ReadOp(ENC28J60_READ_BUF_MEM, 0) << 8;

    // read the packet length (see datasheet page 43)
    len  = ReadOp(ENC28J60_READ_BUF_MEM, 0);
    len |= ReadOp(ENC28J60_READ_BUF_MEM, 0) << 8;
    
    len-=4; //remove the CRC count
    // read the receive status (see datasheet page 43)
    rxstat  = ReadOp(ENC28J60_READ_BUF_MEM, 0);
    rxstat |= ReadOp(ENC28J60_READ_BUF_MEM, 0) << 8;
    // limit retrieve length
    if (len > maxlen - 1)
    {
        len = maxlen - 1;
    }
    
    // check CRC and symbol errors (see datasheet page 44, table 7-3):
    // The ERXFCON.CRCEN is set by default. Normally we should not
    // need to check this.
    if ((rxstat & 0x80)==0)
    {
        // invalid
        len=0;
    }
    else
    {
        // copy the packet from the receive buffer
        ReadBuffer(packet, len);
    }
    // Move the RX read pointer to the start of the next received packet
    // This frees the memory we just read out
    Write(ERXRDPTL, (NextPacketPtr));
    Write(ERXRDPTH, (NextPacketPtr) >> 8);
    
    // decrement the packet counter indicate we are done with this packet
    WriteOp(ENC28J60_BIT_FIELD_SET, ECON2, ECON2_PKTDEC);
    return len;
}
