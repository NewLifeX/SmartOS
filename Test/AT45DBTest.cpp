#include "Drivers\AT45DB.h"

const byte Tx_Buffer[] = "STM32F10x SPI Firmware Library Example: communication with an AT45DB SPI FLASH";
Spi* _spi;

void TestAT45DB()
{
    Spi spi(Spi2, 9000000, true);
    _spi = &spi;
    AT45DB sf(&spi);
    debug_printf("AT45DB ID=%p PageSize=%d\r\n", sf.ID, sf.PageSize);
    int size = ArrayLength(Tx_Buffer);
    debug_printf("DataSize=%d\r\n", size);

    uint addr = 0x00000;
    if(sf.ErasePage(addr))
        debug_printf("擦除0x%08x成功\r\n", addr);
    else
        debug_printf("擦除0x%08x失败\r\n", addr);

    byte Rx_Buffer[80];
    
    for(int i=0; i<size; i++)
    {
        if(Rx_Buffer[i] != Tx_Buffer[i]) debug_printf("Error %d ", i);
    }
    debug_printf("\r\nFinish!\r\n");
}
