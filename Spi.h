#ifndef __SPI_H__
#define __SPI_H__

#include "Sys.h"
#include "Port.h"

#ifdef STM32F10X
	#include "stm32f10x_spi.h"
#else
	#include "stm32f0xx_spi.h"
#endif

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
    OutputPort* _nss;

    AlternatePort* clk;
    AlternatePort* msio;
    AlternatePort* mosi;

public:
    SPI_TypeDef* SPI;
    int Speed;  // 速度
    int Retry;  // 等待重试次数，默认200
    int Error;  // 错误次数

    Spi(int spi, int speedHz = 9000000, bool useNss = true);
    virtual ~Spi();

    byte Write(byte data);
    ushort Write16(ushort data);

    void Start();   // 拉低NSS，开始传输
    void Stop();    // 拉高NSS，停止传输
};

#endif
