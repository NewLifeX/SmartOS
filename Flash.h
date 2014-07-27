#ifndef __SPI_H__
#define __SPI_H__

#include "Sys.h"
#include "Port.h"

// Flash类
class Flash
{
private:
    int _spi;
    OutputPort* _nss;

public:
    SPI_TypeDef* SPI;
    int Speed;  // 速度
    int Retry;  // 等待重试次数，默认200
    int Error;  // 错误次数

    Flash(int spi, int speedHz = 9000000, bool useNss = true);
    virtual ~Flash();

    byte Write(byte data);
    ushort Write16(ushort data);

    void Start();   // 拉低NSS，开始传输
    void Stop();    // 拉高NSS，停止传输
};

#endif
