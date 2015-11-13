#include "Flash.h"
#include <stdlib.h>

void TestFlash()
{
    debug_printf("\r\n\r\n");
    debug_printf("TestFlash Start......\r\n");

    byte buf[] = "STM32F10x SPI Firmware Library Example: communication with an AT45DB SPI FLASH";
    uint size = ArrayLength(buf);

    Flash flash;
	// 凑一个横跨两页的地址
    uint addr = 0x0800C000 + flash.Block - size + 7;
	debug_printf("Address: 0x%08x 凑一个横跨两页的地址\r\n", addr);

    debug_printf("FlashSize: %d(0x%08x) Bytes  Block: %d Bytes\r\n", flash.Size, flash.Size, flash.Block);
    flash.Erase(addr, 0x100);

	byte bb[0x100];
    ByteArray bs;
	bs.Set(bb, ArrayLength(bb));
	bs.SetLength(size);

    flash.Read(addr, bs);

    int n = 0;
    for(int i=0; i<size; i++)
    {
        if(buf[i] != bs[i]) n++;
    }
	if(n)
		debug_printf("分块测试失败 不同点： %d\r\n", n);
	else
		debug_printf("分块测试通过！\r\n");

    // 集成测试
    //flash.Erase(addr, 0x100);
    flash.Write(addr, Array(buf, size));

    flash.Read(addr, bs);

    n = 0;
    for(int i=0; i<size; i++)
    {
        if(buf[i] != bs[i]) n++;
    }
	if(n)
		debug_printf("整体测试失败 不同点： %d\r\n", n);
	else
		debug_printf("整体测试通过！\r\n");

    debug_printf("\r\nTestFlash Finish!\r\n");
}
