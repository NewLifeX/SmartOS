#ifndef __Flash_H__
#define __Flash_H__

#include "Sys.h"
#include "Port.h"

// Flash类
class Flash
{
public:
	bool WriteBlock(uint address, const byte* buf, uint len, bool inc = true);
    // 擦除块 （段地址）
    bool EraseBlock(uint address);
    // 指定块是否被擦除
    bool IsBlockErased(uint address, uint numBytes);

public:
	uint Size;		// 容量大小，字节
    uint Block;		// 每块字节数
    uint Start;		// 起始地址

	Flash();

    // 擦除块。起始地址，字节数量默认0表示擦除全部
    bool Erase(uint address = 0, uint numBytes = 0);
    // 读取段数据 （起始段，字节数量，目标缓冲区）
    bool Read(uint address, ByteArray& bs);
    // 写入段数据 （起始段，段数量，目标缓冲区，读改写）
    bool Write(uint address, const ByteArray& bs, bool readModifyWrite = true);
    bool Memset(uint address, byte data, uint numBytes);
};

#endif
