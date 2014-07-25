#ifndef __SPI_H__
#define __SPI_H__

#include "Sys.h"

/*Spi定义*/
//SPI1..这种格式与st库冲突
#define SPI_1	0
#define SPI_2	1
#define SPI_3	2
#define SPI_NONE 0XFF

// Spi类
class Spi
{
private:
    int _spi;
    Pin _nss;

public:
    int Speed;

    Spi(int spi, int speedMHz = 9, bool useNss = true);
    ~Spi();

    byte ReadWriteByte8(byte data);
    ushort ReadWriteByte16(ushort data);

    void Start();   // 拉低NSS，开始传输
    void Stop();    // 拉高NSS，停止传输
};

#endif
