#ifndef __SPIFLASH_H__
#define __SPIFLASH_H__

#include "Sys.h"
#include "Spi.h"

// SpiFlash
class SpiFlash
{
private:
    Spi* _spi;
    Pin _nss;

	byte ReadByte();
	byte WriteByte(byte data);
	ushort Write(ushort data);
    void SetAddr(uint addr);
	void WaitForEnd();
public:
    int Speed;

    SpiFlash(Spi* spi);
    ~SpiFlash();

    void Erase(uint sector);
	void ErasePage(uint pageAddr);
	void PageWrite(byte* buf, uint addr, uint count);
	void Write(byte* buf, uint addr, uint count);
	void Read(byte* buf, uint addr, uint count);

	uint ReadID();
};

#endif
