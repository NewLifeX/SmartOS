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

public:
    Spi(int spi);
    ~Spi();

    byte ReadWriteByte8(byte data);
    ushort ReadWriteByte16(ushort data);
};

#endif
