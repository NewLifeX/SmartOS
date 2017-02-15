#ifndef __Storage_H__
#define __Storage_H__

#include "Kernel\Sys.h"

// 存储接口
class Storage
{
public:
    // 读取
    virtual bool Read(uint address, Buffer& bs) const = 0;
    // 写入
    virtual bool Write(uint address, const Buffer& bs) const = 0;
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
    virtual bool Read(uint address, Buffer& bs) const;
    // 写入
    virtual bool Write(uint address, const Buffer& bs) const;
	// 清空
    virtual bool Memset(uint address, byte data, uint len) const;
    // 擦除
    virtual bool Erase(uint address, uint len) const;

protected:
	// 写入块
	virtual bool WriteBlock(uint address, const byte* buf, uint len, bool inc) const = 0;
    // 擦除块
    virtual bool EraseBlock(uint address) const = 0;
    // 指定块是否被擦除
    virtual bool IsErased(uint address, uint len) const;
};

// 字符存储接口
class CharStorage : public Storage
{
public:
    // 读取
    virtual bool Read(uint address, Buffer& bs) const;
    // 写入
    virtual bool Write(uint address, const Buffer& bs) const;
};

#endif
