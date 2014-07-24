#ifndef __SPI_H__
#define __SPI_H__

// Spi¿‡
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
