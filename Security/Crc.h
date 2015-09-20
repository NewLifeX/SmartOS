﻿#ifndef __CRC_H__
#define __CRC_H__

#include "Sys.h"

// CRC 校验算法
class Crc
{
public:
    // CRC32校验
    //uint Crc(const void* buf, uint len);
    static uint Hash(const void* buf, uint len, uint crc = 0);
	static ushort Hash16(const void* buf, uint len, ushort crc = 0xFFFF);
};

#endif