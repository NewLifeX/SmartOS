
#ifndef __FLASHFACE_H__
#define __FLASHFACE_H__

#include "Sys.h"

class FlashFace
{
public:
	// 地址，读取数据到字节数组
    virtual bool Read(uint address, ByteArray& bs) = 0;
    // 写入段数据 （起始段，段数量，目标缓冲区，读改写）
    virtual bool Write(uint address, const ByteArray& bs, bool readModifyWrite = true) = 0;
};

#endif //__FLASHFACE_H__
