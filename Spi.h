#ifndef __SPI_H__
#define __SPI_H__

#include "Sys.h"
#include "Port.h"

// Spi类
class Spi
{
private:
    byte _index;
	Pin Pins[4];	// NSS/CLK/MISO/MOSI
    OutputPort _nss;

    AlternatePort _clk;
    AlternatePort _miso;
    AlternatePort _mosi;

	void Init();

public:
    SPI_TypeDef* SPI;
    int Speed;  // 速度
    int Retry;  // 等待重试次数，默认200
    int Error;  // 错误次数
	bool Opened;

	Spi();
	// 使用端口和最大速度初始化Spi，因为需要分频，实际速度小于等于该速度
    Spi(SPI_TypeDef* spi, uint speedHz = 9000000, bool useNss = true);
    ~Spi();
	
	void Init(SPI_TypeDef* spi, uint speedHz = 9000000, bool useNss = true);

	void SetPin(Pin clk = P0, Pin miso = P0, Pin mosi = P0, Pin nss = P0);
	void GetPin(Pin* clk = NULL, Pin* miso = NULL, Pin* mosi = NULL, Pin* nss = NULL);
	void Open();
	void Close();

	// 基础读写
    byte Write(byte data);
    ushort Write16(ushort data);

	// 批量读写。以字节数组长度为准
	void Write(const ByteArray& bs);
	void Read(ByteArray& bs);
	
    void Start();   // 拉低NSS，开始传输
    void Stop();    // 拉高NSS，停止传输
};

// Spi会话类。初始化时打开Spi，超出作用域析构时关闭
class SpiScope
{
private:
	Spi* _spi;

public:
	_force_inline SpiScope(Spi* spi)
	{
		_spi = spi;
		_spi->Start();
	}

	_force_inline ~SpiScope()
	{
		_spi->Stop();
	}
};

#endif
