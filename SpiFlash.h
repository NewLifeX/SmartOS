#ifndef __SPIFLASH_H__
#define __SPIFLASH_H__

#include "Sys.h"
#include "Spi.h"

// SpiFlash
class SpiFlash
{
private:
    Spi* _spi;  // 内部Spi对象

    // 设置操作地址
    void SetAddr(uint addr);
    // 等待操作完成
	void WaitForEnd();
public:
    ushort PageSize;    // 页大小

    SpiFlash(Spi* spi);
    ~SpiFlash();

    // 擦除扇区
    void Erase(uint sector);
    // 擦除页
	void ErasePage(uint pageAddr);
    // 按页写入
	void WritePage(uint addr, byte* buf, uint count);
    // 写入数据
	void Write(uint addr, byte* buf, uint count);
    // 读取数据
	void Read(uint addr, byte* buf, uint count);

    // 读取编号
	uint ReadID();
};

#endif
