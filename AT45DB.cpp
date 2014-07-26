#include "Sys.h"
#include "Port.h"
#include "Spi.h"
#include "AT45DB.h"

#define READ       0xD2  /* Read from Memory instruction */
#define WRITE      0x82  /* Write to Memory instruction */

#define RDID       0x9F  /* Read identification */
#define RDSR       0xD7  /* Read Status Register instruction  */

#define SE         0x7C  /* Sector Erase instruction */
#define PE         0x81  /* Page Erase instruction */

#define RDY_Flag   0x80  /* Ready/busy(1/0) status flag */

#define Dummy_Byte 0xA5

AT45DB::AT45DB(Spi* spi)
{
    _spi = spi;
    _spi->Stop();

    //PageSize = 0x210;
    
    // 状态位最后一位决定页大小
    _spi->Start();
    byte status = _spi->WriteRead(Dummy_Byte) & 0x01;
    _spi->Stop();
    
    // 需要根据芯片来动态识别页大小
    ID = ReadID();
    if(ID == 0x1F260000)
        PageSize = status ? 512 : 528;
    else if(ID == 0x1F240000)
        PageSize = status ? 256 : 264;
    else
    {
        PageSize = 256;
        if(Sys.Debug) printf("AT45DB Not Support 0x%08X\r\n", ID);
    }
}

AT45DB::~AT45DB()
{
    delete _spi;
}

// 设置操作地址
void AT45DB::SetAddr(uint addr)
{
    /* Send addr high nibble address byte */
    _spi->WriteRead((addr & 0xFF0000) >> 16);
    /* Send addr medium nibble address byte */
    _spi->WriteRead((addr & 0xFF00) >> 8);
    /* Send addr low nibble address byte */
    _spi->WriteRead(addr & 0xFF);
}

// 等待操作完成
void AT45DB::WaitForEnd()
{
    uint8_t FLASH_Status = 0;

    _spi->Start();

    /* Send "Read Status Register" instruction */
    _spi->WriteRead(RDSR);

    /* Loop as long as the memory is busy with a write cycle */
    do
    {
        /* Send a dummy byte to generate the clock needed by the FLASH
        and put the value of the status register in FLASH_Status variable */
        FLASH_Status = _spi->WriteRead(Dummy_Byte);
    }
    while ((FLASH_Status & RDY_Flag) == RESET); /* Busy in progress */

    _spi->Stop();
}

// 擦除扇区
void AT45DB::Erase(uint sector)
{
    _spi->Start();
    /* Send Sector Erase instruction */
    _spi->WriteRead(SE);
    SetAddr(sector);
    _spi->Stop();

    /* Wait the end of Flash writing */
    WaitForEnd();
}

// 擦除页
void AT45DB::ErasePage(uint pageAddr)
{
    _spi->Start();
    /* Send Sector Erase instruction */
    _spi->WriteRead(PE);
    SetAddr(pageAddr);
    _spi->Stop();

    WaitForEnd();
}

// 按页写入
void AT45DB::WritePage(uint addr, byte* buf, uint count)
{
    _spi->Start();
    /* Send "Write to Memory " instruction */
    _spi->WriteRead(WRITE);
    SetAddr(addr);

    /* while there is data to be written on the FLASH */
    while (count--)
    {
        /* Send the current byte */
        _spi->WriteRead(*buf);
        /* Point on the next byte to be written */
        buf++;
    }

    _spi->Stop();

    WaitForEnd();
}

// 写入数据
void AT45DB::Write(uint addr, byte* buf, uint count)
{
    // 地址中不够一页的部分
    byte addr2 = addr % PageSize;
    // 页数
    byte pages =  count / PageSize;
    // 字节数中不够一页的部分
    byte singles = count % PageSize;

    if (addr2 == 0) /* 地址按页大小对齐 */
    {
        if (pages == 0) /* count < PageSize */
        {
            WritePage(addr, buf, count);
        }
        else /* count > PageSize */
        {
            // 逐页写入
            while (pages--)
            {
                WritePage(addr, buf, PageSize);
                addr +=  PageSize;
                buf += PageSize;
            }

            WritePage(addr, buf, singles);
        }
    }
    else /* 地址不是页大小对齐 */
    {
        byte num = PageSize - addr2;
        if (pages == 0) /* count < PageSize */
        {
            // 如果要写入的数据跨两页，则需要分两次写入
            if (singles > num) /* (count + addr) > PageSize */
            {
                byte temp = singles - num;

                WritePage(addr, buf, num);
                addr +=  num;
                buf += num;

                WritePage(addr, buf, temp);
            }
            else
            {
                WritePage(addr, buf, count);
            }
        }
        else /* count > PageSize */
        {
            count -= num;
            pages =  count / PageSize;
            singles = count % PageSize;

            WritePage(addr, buf, num);
            addr +=  num;
            buf += num;

            while (pages--)
            {
                WritePage(addr, buf, PageSize);
                addr +=  PageSize;
                buf += PageSize;
            }

            if (singles != 0)
            {
                WritePage(addr, buf, singles);
            }
        }
    }
}

// 读取数据
void AT45DB::Read(uint addr, byte* buf, uint count)
{
    _spi->Start();

    /* Send "Read from Memory " instruction */
    _spi->WriteRead(READ);

    SetAddr(addr);

    _spi->WriteRead(Dummy_Byte);
    _spi->WriteRead(Dummy_Byte);
    _spi->WriteRead(Dummy_Byte);
    _spi->WriteRead(Dummy_Byte);

    while (count--) /* while there is data to be read */
    {
        /* Read a byte from the FLASH */
        *buf = _spi->WriteRead(Dummy_Byte);
        /* Point to the next location where the byte read will be saved */
        buf++;
    }

    _spi->Stop();
}

// 读取编号
uint AT45DB::ReadID()
{
    _spi->Start();

    /* Send "RDID " instruction */
    _spi->WriteRead(RDID);

    uint Temp0 = _spi->WriteRead(Dummy_Byte);
    uint Temp1 = _spi->WriteRead(Dummy_Byte);
    uint Temp2 = _spi->WriteRead(Dummy_Byte);
    uint Temp3 = _spi->WriteRead(Dummy_Byte);

    _spi->Stop();

    return (Temp0 << 24) | (Temp1 << 16) | (Temp2<<8) | Temp3;
}
