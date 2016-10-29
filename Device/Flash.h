#ifndef __Flash_H__
#define __Flash_H__

#include "Storage\Storage.h"

// Flash类
class Flash : public BlockStorage
{
public:
	// 写块
	virtual bool WriteBlock(uint address, const byte* buf, uint len, bool inc) const;
    // 擦除块 （段地址）
    virtual bool EraseBlock(uint address) const;

public:
	Flash();
	// 设置读保护   注意：解除读保护时会擦除所有 Flash 内容
	static bool ReadOutProtection(bool set);

private:
	void OnInit();
};

#endif
