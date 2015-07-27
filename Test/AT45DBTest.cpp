#include "Drivers\AT45DB.h"

const byte Tx_Buffer[] = "STM32F10x SPI Firmware Library Example: communication with an AT45DB SPI FLASH";
Spi* _spi;

#define READ       0xD2  /* Read from Memory instruction */
#define WRITE      0x82  /* Write to Memory instruction */

#define RDID       0x9F  /* Read identification */
#define RDSR       0xD7  /* Read Status Register instruction  */

#define SE         0x7C  /* Sector Erase instruction */
#define PE         0x81  /* Page Erase instruction */

#define RDY_Flag   0x80  /* Ready/busy(1/0) status flag */

#define Dummy_Byte 0xA5

void SPI_FLASH_WaitForEnd(void)
{
  uint8_t FLASH_Status = 0;

  _spi->Start();

  /* Send "Read Status Register" instruction */
  _spi->Write(RDSR);

  /* Loop as long as the memory is busy with a write cycle */
  do
  {
    /* Send a dummy byte to generate the clock needed by the FLASH
    and put the value of the status register in FLASH_Status variable */
    FLASH_Status = _spi->Write(Dummy_Byte);

  }
  while ((FLASH_Status & RDY_Flag) == RESET); /* Busy in progress */

  _spi->Stop();
}

void SPI_FLASH_PageWrite(uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite)
{
  _spi->Start();
  /* Send "Write to Memory " instruction */
  _spi->Write(WRITE);
  /* Send WriteAddr high nibble address byte to write to */
  _spi->Write((WriteAddr & 0xFF0000) >> 16);
  /* Send WriteAddr medium nibble address byte to write to */
  _spi->Write((WriteAddr & 0xFF00) >> 8);
  /* Send WriteAddr low nibble address byte to write to */
  _spi->Write(WriteAddr & 0xFF);

  /* while there is data to be written on the FLASH */
  while (NumByteToWrite--)
  {
    /* Send the current byte */
    _spi->Write(*pBuffer);
    /* Point on the next byte to be written */
    pBuffer++;
  }

  _spi->Stop();

  SPI_FLASH_WaitForEnd();
}

void SPI_FLASH_BufferRead(uint8_t* pBuffer, uint32_t ReadAddr, uint16_t NumByteToRead)
{
  _spi->Start();

  /* Send "Read from Memory " instruction */
  _spi->Write(READ);

  /* Send ReadAddr high nibble address byte to read from */
  _spi->Write((ReadAddr & 0xFF0000) >> 16);
  /* Send ReadAddr medium nibble address byte to read from */
  _spi->Write((ReadAddr& 0xFF00) >> 8);
  /* Send ReadAddr low nibble address byte to read from */
  _spi->Write(ReadAddr & 0xFF);

  /* Read a byte from the FLASH */
  _spi->Write(Dummy_Byte);
  /* Read a byte from the FLASH */
  _spi->Write(Dummy_Byte);
  /* Read a byte from the FLASH */
  _spi->Write(Dummy_Byte);
  /* Read a byte from the FLASH */
  _spi->Write(Dummy_Byte);

  while (NumByteToRead--) /* while there is data to be read */
  {
    /* Read a byte from the FLASH */
    *pBuffer = _spi->Write(Dummy_Byte);
    /* Point to the next location where the byte read will be saved */
    pBuffer++;
  }

    _spi->Stop();
}

void TestAT45DB()
{
    Spi spi(SPI2, 9000000, true);
    _spi = &spi;
    AT45DB sf(&spi);
    debug_printf("AT45DB ID=0x%08X PageSize=%d\r\n", sf.ID, sf.PageSize);
    int size = ArrayLength(Tx_Buffer);
    debug_printf("DataSize=%d\r\n", size);

    uint addr = 0x00000;
    if(sf.ErasePage(addr))
        debug_printf("擦除0x%08x成功\r\n", addr);
    else
        debug_printf("擦除0x%08x失败\r\n", addr);

    byte Rx_Buffer[80];
    for(int i=0; i<9; i++)
    {
        //sf.ErasePage(addr);
        SPI_FLASH_PageWrite((byte*)Tx_Buffer, addr, size);
        //SPI_FLASH_BufferRead(Rx_Buffer, addr, size);
        //memset(Rx_Buffer, 0, ArrayLength(Rx_Buffer));
        //if(!sf.Write(addr, Tx_Buffer, size)) debug_printf("写入0x%08X失败！\r\n", addr);
        SPI_FLASH_BufferRead(Rx_Buffer, addr, size);
        //if(!sf.ReadPage(addr, Rx_Buffer, size)) debug_printf("读取0x%08X失败！\r\n", addr);
        memset(Rx_Buffer, 0, ArrayLength(Rx_Buffer));
        addr += size;
    }
    
    for(int i=0; i<size; i++)
    {
        if(Rx_Buffer[i] != Tx_Buffer[i]) debug_printf("Error %d ", i);
    }
    debug_printf("\r\nFinish!\r\n");
}
