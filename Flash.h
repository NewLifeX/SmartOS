#ifndef __SPI_H__
#define __SPI_H__

#include "Sys.h"
#include "Port.h"

// Flash类
class Flash
{
private:
	bool WriteBlock(byte* address, uint numBytes, byte* pSectorBuff, bool fIncrementDataPtr);
    // 擦除块 （段地址）
    bool EraseBlock(byte* sector);
    // 指定块是否被擦除
    bool IsBlockErased(byte* blockStart, uint blockLength);

public:
	uint Size;			// 容量大小，字节
    uint BytesPerBlock;	// 每块字节数

	Flash();

    // 擦除块。其实地址，字节数量默认0表示擦除全部
    bool Erase(byte* address = 0, uint numBytes = 0);
    // 读取段数据 （起始段，字节数量，目标缓冲区）
    bool Read(byte* address, uint numBytes, byte* buf);
    // 写入段数据 （起始段，段数量，目标缓冲区，读改写）
    bool Write(byte* address, uint numBytes, byte* pSectorBuff, bool readModifyWrite);
    bool Memset(byte* address, byte data, uint numBytes);
};

#endif
