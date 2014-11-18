#include "Enc28j60.h"

#define ENC_DEBUG 0

Enc28j60::Enc28j60() { Init(); }

Enc28j60::Enc28j60(Spi* spi, Pin ce/*, Pin irq*/)
{
    //_spi = spi;
	//_ce = NULL;
    //if(ce != P0) _ce = new OutputPort(ce, false, false);
	Init();
	Init(spi, ce);
}

Enc28j60::~Enc28j60()
{
	delete _spi;
	_spi = NULL;

	/*if(_ce) delete _ce;
	_ce = NULL;*/
}

void Enc28j60::Init()
{
	_spi = NULL;
}

void Enc28j60::Init(Spi* spi, Pin ce)
{
	_spi = spi;
	if(ce != P0)
	{
		_ce.OpenDrain = false;
		_ce.Set(ce);
	}
}

byte Enc28j60::ReadOp(byte op, byte addr)
{
    SpiScope sc(_spi);

    _spi->Write(op | (addr & ADDR_MASK));
    byte dat = _spi->Write(0xFF);
    // do dummy read if needed (for mac and mii, see datasheet page 29)
    if(addr & 0x80)
    {
        dat = _spi->Write(0xFF);
    }

    return dat;
}

void Enc28j60::WriteOp(byte op, byte addr, byte data)
{
    SpiScope sc(_spi);

    _spi->Write(op | (addr & ADDR_MASK));
    _spi->Write(data);
}

void Enc28j60::ReadBuffer(byte* buf, uint len)
{
    SpiScope sc(_spi);

    _spi->Write(ENC28J60_READ_BUF_MEM);
    while(len--)
    {
        *buf++ = _spi->Write(0);
    }
    *buf='\0';
}

void Enc28j60::WriteBuffer(const byte* buf, uint len)
{
    SpiScope sc(_spi);

    _spi->Write(ENC28J60_WRITE_BUF_MEM);
    while(len--)
    {
        _spi->Write(*buf++);
    }
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

byte Enc28j60::ReadReg(byte addr)
{
    SetBank(addr);
    return ReadOp(ENC28J60_READ_CTRL_REG, addr);
}

void Enc28j60::WriteReg(byte addr, byte data)
{
    SetBank(addr);
    WriteOp(ENC28J60_WRITE_CTRL_REG, addr, data);
}

// 发送ARP请求包到目的地址
uint Enc28j60::PhyRead(byte addr)
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

bool Enc28j60::PhyWrite(byte addr, uint data)
{
    // set the PHY register addr
    WriteReg(MIREGADR, addr);
    // write the PHY data
    WriteReg(MIWRL, data);
    WriteReg(MIWRH, data >> 8);

	//ulong us = Time.Current() + 10 * 1000;
	TimeWheel tw(0, 10, 0);
    // 等待 PHY 写完成
    while(ReadReg(MISTAT) & MISTAT_BUSY)
    {
        //if(us < Time.Current()) return false;
		if(tw.Expired()) return false;
    }
	return true;
}

void Enc28j60::ClockOut(byte clock)
{
    // setup clkout: 2 is 12.5MHz:
    WriteReg(ECOCON, clock & 0x7);
}

void Enc28j60::Init(byte mac[6])
{
	assert_param(mac);
	
	memcpy(Mac, mac, 6);
}

bool Enc28j60::OnOpen()
{
	assert_param(Mac);

	debug_printf("Enc28j60::Open(%02X-%02X-%02X-%02X-%02X-%02X)\r\n", Mac[0], Mac[1], Mac[2], Mac[3], Mac[4], Mac[5]);
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
    WriteOp(ENC28J60_SOFT_RESET, 0, ENC28J60_SOFT_RESET);
	Sys.Sleep(3);

    // check CLKRDY bit to see if reset is complete
    // The CLKRDY does not work. See Rev. B4 Silicon Errata point. Just wait.
    //while(!(ReadReg(ESTAT) & ESTAT_CLKRDY));
    // do bank 0 stuff
    // initialize receive buffer
    // 16-bit transfers, must write low byte first
    // 设置接收缓冲区开始地址
    NextPacketPtr = RXSTART_INIT;
    // Rx开始
    WriteReg(ERXSTL, RXSTART_INIT & 0xFF);
    WriteReg(ERXSTH, RXSTART_INIT >> 8);
    // 设置接收指针地址
    WriteReg(ERXRDPTL, RXSTART_INIT & 0xFF);
    WriteReg(ERXRDPTH, RXSTART_INIT >> 8);
    // 设置接收缓冲区的末尾地址 ERXND寄存器默认指向整个缓冲区的最后一个单元
    WriteReg(ERXNDL, RXSTOP_INIT & 0xFF);
    WriteReg(ERXNDH, RXSTOP_INIT >> 8);
    // 设置发送缓冲区起始地址 ETXST寄存器默认地址是整个缓冲区的第一个单元
    WriteReg(ETXSTL, TXSTART_INIT & 0xFF);
    WriteReg(ETXSTH, TXSTART_INIT >> 8);
    // TX 结束
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

    //WriteReg(ERXFCON, ERXFCON_UCEN|ERXFCON_CRCEN|ERXFCON_PMEN);
    WriteReg(ERXFCON, ERXFCON_UCEN | ERXFCON_CRCEN | ERXFCON_BCEN); // ERXFCON_BCEN 不过滤广播包，实现DHCP
    WriteReg(EPMM0, 0x3f);
    WriteReg(EPMM1, 0x30);
    WriteReg(EPMCSL, 0xf9);
    WriteReg(EPMCSH, 0xf7);

    // Bank 2，打开MAC接收
    WriteReg(MACON1, MACON1_MARXEN | MACON1_TXPAUS | MACON1_RXPAUS);
    // MACON2清零，让MAC退出复位状态
    WriteReg(MACON2, 0x00);
    // 启用自动填充到60字节并进行Crc校验
    WriteOp(ENC28J60_BIT_FIELD_SET, MACON3, MACON3_PADCFG0 | MACON3_TXCRCEN | MACON3_FRMLNEN | MACON3_FULDPX);
    // 配置非背对背包之间的间隔
    WriteReg(MAIPGL, 0x12);
    WriteReg(MAIPGH, 0x0C);
    // 配置背对背包之间的间隔
    WriteReg(MABBIPG, 0x15);   // 有的例程这里是0x12
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

	bool flag = true;
    // 配置PHY为全双工  LEDB为拉电流
    if(flag && !PhyWrite(PHCON1, PHCON1_PDPXMD)) flag = false;
    // 阻止发送回路的自动环回
    if(flag && !PhyWrite(PHCON2, PHCON2_HDLDIS)) flag = false;
    // PHY LED 配置,LED用来指示通信的状态
    if(flag && !PhyWrite(PHLCON, 0x476)) flag = false;
	if(!flag)
	{
		debug_printf("Enc28j60::Init Failed! Can't write Physical, please check Spi!\r\n");
		return false;
	}

    // 切换到bank0
    SetBank(ECON1);
    // 打开中断
    WriteOp(ENC28J60_BIT_FIELD_SET, EIE, EIE_INTIE | EIE_PKTIE);
    // 新增加，有些例程里面没有
    WriteOp(ENC28J60_BIT_FIELD_SET, EIE, EIE_RXERIE | EIE_TXERIE | EIE_INTIE);
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

	debug_printf("Enc28j60::Inited! Revision=%d\r\n", rev);
	return true;
}

byte Enc28j60::GetRevision()
{
    // 在EREVID 内也存储了版本信息。 EREVID 是一个只读控制寄存器，包含一个5 位标识符，用来标识器件特定硅片的版本号
    return ReadReg(EREVID);
}

bool Enc28j60::OnWrite(const byte* packet, uint len)
{
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

    // 把传输缓冲区的内容发送到网络
    WriteOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_TXRTS);

#if ENC_DEBUG
    if(GetRevision() == 0x05u || GetRevision() == 0x06u)
	{
		ushort count = 0;
		while((ReadReg(EIR) & (EIR_TXERIF | EIR_TXIF)) && (++count < 1000));
		if((ReadReg(EIR) & EIR_TXERIF) || (count >= 1000))
		{
			WORD_VAL ReadPtrSave;
			WORD_VAL TXEnd;
			TXSTATUS TXStatus;
			byte i;

			// Cancel the previous transmission if it has become stuck set
			//BFCReg(ECON1, ECON1_TXRTS);
            WriteOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_TXRTS);

			// Save the current read pointer (controlled by application)
			ReadPtrSave.v[0] = ReadReg(ERDPTL);
			ReadPtrSave.v[1] = ReadReg(ERDPTH);

			// Get the location of the transmit status vector
			TXEnd.v[0] = ReadReg(ETXNDL);
			TXEnd.v[1] = ReadReg(ETXNDH);
			TXEnd.Val++;

			// ReadReg the transmit status vector
			WriteReg(ERDPTL, TXEnd.v[0]);
			WriteReg(ERDPTH, TXEnd.v[1]);

			ReadBuffer((byte*)&TXStatus, sizeof(TXStatus));

			// Implement retransmission if a late collision occured (this can
			// happen on B5 when certain link pulses arrive at the same time
			// as the transmission)
			for(i = 0; i < 16u; i++)
			{
				if((ReadReg(EIR) & EIR_TXERIF) && TXStatus.bits.LateCollision)
				{
					// Reset the TX logic
                    WriteOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_TXRTS);
                    WriteOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_TXRTS);
                    WriteOp(ENC28J60_BIT_FIELD_CLR, EIR, EIR_TXERIF | EIR_TXIF);

					// Transmit the packet again
					WriteOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_TXRTS);
					while(!(ReadReg(EIR) & (EIR_TXERIF | EIR_TXIF)));

					// Cancel the previous transmission if it has become stuck set
					WriteOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_TXRTS);

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

			// Restore the current read pointer
			WriteReg(ERDPTL, ReadPtrSave.v[0]);
			WriteReg(ERDPTH, ReadPtrSave.v[1]);
		}
	}
#endif

    // Reset the transmit logic problem. See Rev. B4 Silicon Errata point 12.
    if( (ReadReg(EIR) & EIR_TXERIF) )
    {
        WriteOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_TXRTS);
    }

	return true;
}

// 从网络接收缓冲区获取一个数据包，该包开头是以太网头
// packet，该包应该存储到的缓冲区；maxlen，可接受的最大数据长度
uint Enc28j60::OnRead(byte* packet, uint maxlen)
{
    uint rxstat;
    uint len;

    // 检测缓冲区是否收到一个数据包
    /*if( !(ReadReg(EIR) & EIR_PKTIF) )
	{
		// The above does not work. See Rev. B4 Silicon Errata point 6.
		// 通过查看EPKTCNT寄存器再次检查是否收到包
		// EPKTCNT为0表示没有包接收/或包已被处理
		if(ReadReg(EPKTCNT) == 0) return 0;
	}*/

	// 收到的以太网数据包长度
    if( ReadReg(EPKTCNT) == 0 ) return 0;

    // 配置接收缓冲器读指针指向地址
    WriteReg(ERDPTL, (NextPacketPtr));
    WriteReg(ERDPTH, (NextPacketPtr) >> 8);

    // 下一个数据包的读指针
    NextPacketPtr  = ReadOp(ENC28J60_READ_BUF_MEM, 0);
    NextPacketPtr |= ReadOp(ENC28J60_READ_BUF_MEM, 0) << 8;

    // 读数据包字节长度 (see datasheet page 43)
    len  = ReadOp(ENC28J60_READ_BUF_MEM, 0);
    len |= ReadOp(ENC28J60_READ_BUF_MEM, 0) << 8;

    len -= 4; // 删除 CRC 计数
    // 读接收数据包的状态 (see datasheet page 43)
    rxstat  = ReadOp(ENC28J60_READ_BUF_MEM, 0);
    rxstat |= ReadOp(ENC28J60_READ_BUF_MEM, 0) << 8;
    // 限制获取的长度。有些例程这里不用减一
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
        len = 0;
    }
    else
    {
        // 从缓冲区中将数据包复制到packet中
        ReadBuffer(packet, len);
    }
    // Move the RX read pointer to the start of the next received packet
    // This frees the memory we just read out
    WriteReg(ERXRDPTL, (NextPacketPtr));
    WriteReg(ERXRDPTH, (NextPacketPtr) >> 8);

    // 数据包个数递减位EPKTCNT减1
    WriteOp(ENC28J60_BIT_FIELD_SET, ECON2, ECON2_PKTDEC);

    return len;
}

// 返回MAC连接状态
bool Enc28j60::Linked()
{
	return PhyRead(PHSTAT1) & PHSTAT1_LLSTAT;
}
