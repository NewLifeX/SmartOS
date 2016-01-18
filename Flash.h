#ifndef __Flash_H__
#define __Flash_H__

#include "Sys.h"
#include "Port.h"
#include "Storage\Storage.h"

// Flash类
class Flash : public BlockStorage
{
public:
	virtual bool WriteBlock(uint address, const byte* buf, uint len, bool inc) const;
    // 擦除块 （段地址）
    virtual bool EraseBlock(uint address) const;

public:
	Flash();
};

#endif
