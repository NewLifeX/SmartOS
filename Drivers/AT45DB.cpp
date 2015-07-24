#include "Sys.h"
#include "Port.h"
#include "Spi.h"
#include "AT45DB.h"

#define Dummy_Byte 0xA5

AT45DB::AT45DB(Spi* spi)
{
    _spi = spi;
    _spi->Stop();

    // 在120M主频下用3000，其它主频不确定是否需要调整
    Retry = 3000;
    //PageSize = 0x210;

    SpiScope sc(_spi);
    // 状态位最后一位决定页大小
    // 读状态寄存器命令
    _spi->Write(0xD7);
    byte status = _spi->Write(Dummy_Byte) & 0x01;
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
        debug_printf("AT45DB Not Support 0x%08X\r\n", ID);
    }
}

AT45DB::~AT45DB()
{
    if(_spi) delete _spi;
    _spi = NULL;
}

// 设置操作地址
void AT45DB::SetAddr(uint addr)
{
    /* Send addr high nibble address byte */
    _spi->Write((addr & 0xFF0000) >> 16);
    /* Send addr medium nibble address byte */
    _spi->Write((addr & 0xFF00) >> 8);
    /* Send addr low nibble address byte */
    _spi->Write(addr & 0xFF);
}

// 等待操作完成
bool AT45DB::WaitForEnd()
{
    SpiScope sc(_spi);

    // 读状态寄存器命令
    _spi->Write(0xD7);

    int retry = Retry;
    byte status = 0;
    /* Loop as long as the memory is busy with a write cycle */
    do
    {
        /* Send a dummy byte to generate the clock needed by the FLASH
        and put the value of the status register in FLASH_Status variable */
        status = _spi->Write(Dummy_Byte);
    }
    while ((status & 0x80) == RESET && --retry); /* Busy in progress */

    // 重试次数没有用完，才返回成功
    return retry > 0;
}

// 擦除扇区
bool AT45DB::Erase(uint sector)
{
    SpiScope sc(_spi);
	// 扇区擦除命令
    _spi->Write(0x7C);
    SetAddr(sector);

    return WaitForEnd();
}

// 擦除页
bool AT45DB::ErasePage(uint pageAddr)
{
    SpiScope sc(_spi);
	// 页擦除命令
    _spi->Write(0x81);
    SetAddr(pageAddr);

    return WaitForEnd();
}

// 按页写入
bool AT45DB::WritePage(uint addr, byte* buf, uint count)
{
    SpiScope sc(_spi);

	// 主存储器页编程命令，带预擦除
    _spi->Write(0x82);
    SetAddr(addr);

    while (count--)
    {
        _spi->Write(*buf);
        buf++;
    }

    //WaitForEnd();
    // 使用第二缓冲区来作为写入
    /*_spi->Write(0x87);

    _spi->Write(Dummy_Byte);
    _spi->Write(Dummy_Byte);
    _spi->Write(Dummy_Byte);
    _spi->Write(Dummy_Byte);

    // 把数据写入缓冲区
    while (count--)
    {
        _spi->Write(*buf);
        buf++;
    }
    _spi->Stop();
    //WaitForEnd();

    SpiScope sc(_spi);
    // 将第二缓冲区的数据写入主存储器（擦除模式）
    _spi->Write(0x86);
    SetAddr(addr & 0xFFFF00);*/

    return WaitForEnd();
}

// 写入数据
bool AT45DB::Write(uint addr, byte* buf, uint count)
{
    // 地址中不够一页的部分
    uint addr2 = addr % PageSize;
    // 页数
    uint pages =  count / PageSize;
    // 字节数中不够一页的部分
    uint singles = count % PageSize;

    if (addr2 == 0) /* 地址按页大小对齐 */
    {
        if (pages == 0) /* count < PageSize */
        {
            return WritePage(addr, buf, count);
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

            return WritePage(addr, buf, singles);
        }
    }
    else /* 地址不是页大小对齐 */
    {
        uint num = PageSize - addr2;
        if (pages == 0) /* count < PageSize */
        {
            // 如果要写入的数据跨两页，则需要分两次写入
            if (singles > num) /* (count + addr) > PageSize */
            {
                byte temp = singles - num;

                WritePage(addr, buf, num);
                addr +=  num;
                buf += num;

                return WritePage(addr, buf, temp);
            }
            else
            {
                return WritePage(addr, buf, count);
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

            return true;
        }
    }
}

// 读取一页
bool AT45DB::ReadPage(uint addr, byte* buf, uint count)
{
    SpiScope sc(_spi);

    // 主存储器页读取命令
    _spi->Write(0xD2);
    // 使用第二缓冲区来读取
    //_spi->Write(0xD3);

    SetAddr(addr);

    _spi->Write(Dummy_Byte);
    _spi->Write(Dummy_Byte);
    _spi->Write(Dummy_Byte);
    _spi->Write(Dummy_Byte);

    while (count--)
    {
        *buf = _spi->Write(Dummy_Byte);
        buf++;
    }

    return true;
}

// 读取数据
bool AT45DB::Read(uint addr, byte* buf, uint count)
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
            return ReadPage(addr, buf, count);
        }
        else /* count > PageSize */
        {
            // 逐页写入
            while (pages--)
            {
                ReadPage(addr, buf, PageSize);
                addr +=  PageSize;
                buf += PageSize;
            }

            return ReadPage(addr, buf, singles);
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

                ReadPage(addr, buf, num);
                addr +=  num;
                buf += num;

                return ReadPage(addr, buf, temp);
            }
            else
            {
                return ReadPage(addr, buf, count);
            }
        }
        else /* count > PageSize */
        {
            count -= num;
            pages =  count / PageSize;
            singles = count % PageSize;

            ReadPage(addr, buf, num);
            addr +=  num;
            buf += num;

            while (pages--)
            {
                ReadPage(addr, buf, PageSize);
                addr +=  PageSize;
                buf += PageSize;
            }

            if (singles != 0)
            {
                ReadPage(addr, buf, singles);
            }

            return true;
        }
    }
}

// 读取编号
uint AT45DB::ReadID()
{
    SpiScope sc(_spi);

    // 读ID命令，AT45DB161D的ID是1F260000；其中：1F代表厂商ID；2600代表设备ID；00代表配置符字节数
    _spi->Write(0x9F);

    uint Temp0 = _spi->Write(Dummy_Byte);
    uint Temp1 = _spi->Write(Dummy_Byte);
    uint Temp2 = _spi->Write(Dummy_Byte);
    uint Temp3 = _spi->Write(Dummy_Byte);

    return (Temp0 << 24) | (Temp1 << 16) | (Temp2<<8) | Temp3;
}
