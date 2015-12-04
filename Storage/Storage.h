#ifndef __Storage_H__
#define __Storage_H__

#include "Sys.h"

// 存储接口
class Storage
{
public:
    // 读取
    virtual bool Read(uint address, Array& bs) = 0;
    // 写入
    virtual bool Write(uint address, const Array& bs) = 0;
};

// 块存储接口
class BlockStorage : public Storage
{
public:
	uint Size;		// 容量大小，字节
    uint Start;		// 起始地址
    uint Block;		// 每块字节数
	bool ReadModifyWrite;	// 是否读改写

    // 读取
    virtual bool Read(uint address, Array& bs);
    // 写入
    virtual bool Write(uint address, const Array& bs);
	// 清空
    virtual bool Memset(uint address, byte data, uint len);
    // 擦除
    virtual bool Erase(uint address, uint len);

protected:
	// 写入块
	virtual bool WriteBlock(uint address, const byte* buf, uint len, bool inc) = 0;
    // 擦除块
    virtual bool EraseBlock(uint address) = 0;
    // 指定块是否被擦除
    virtual bool IsErased(uint address, uint len);
};

// 字符存储接口
class CharStorage : public Storage
{
public:
    // 读取
    virtual bool Read(uint address, Array& bs);
    // 写入
    virtual bool Write(uint address, const Array& bs);
};

#endif
