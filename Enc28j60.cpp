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

	// 使能单播过滤 使能CRC校验 使能 格式匹配自动过滤
    //WriteReg(ERXFCON, ERXFCON_UCEN|ERXFCON_CRCEN|ERXFCON_PMEN);
    WriteReg(ERXFCON, ERXFCON_UCEN | ERXFCON_CRCEN | ERXFCON_BCEN); // ERXFCON_BCEN 不过滤广播包，实现DHCP
    WriteReg(EPMM0, 0x3f);
    WriteReg(EPMM1, 0x30);
    WriteReg(EPMCSL, 0xf9);
    WriteReg(EPMCSH, 0xf7);

    // Bank 2，使能MAC接收 允许MAC发送暂停控制帧 当接收到暂停控制帧时停止发送
    WriteReg(MACON1, MACON1_MARXEN | MACON1_TXPAUS | MACON1_RXPAUS);
    // MACON2清零，让MAC退出复位状态
    WriteReg(MACON2, 0x00);
    // 启用自动填充到60字节并进行Crc校验 帧长度校验使能 MAC全双工使能
	// 提示 由于ENC28J60不支持802.3的自动协商机制， 所以对端的网络卡需要强制设置为全双工
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
    // 使能中断 全局中断 接收中断 接收错误中断
    WriteOp(ENC28J60_BIT_FIELD_SET, EIE, EIE_INTIE | EIE_PKTIE | EIE_RXERIE);
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
	if(!Linked())
	{
		debug_printf("以太网已断开！\r\n");
		return false;
	}

	while((ReadReg(ECON1) & ECON1_TXRTS) != 0);

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

    // 复位发送逻辑的问题
    if(ReadReg(EIR) & EIR_TXERIF)
    {
		SetBank(ECON1);
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

    // 数据包个数递减位EPKTCNT减1
    WriteOp(ENC28J60_BIT_FIELD_SET, ECON2, ECON2_PKTDEC);

    return len;
}

// 返回MAC连接状态
bool Enc28j60::Linked()
{
	return PhyRead(PHSTAT1) & PHSTAT1_LLSTAT;
}
