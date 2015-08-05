#include "SerialPort.h"

uint OnUsartRead(ITransport* transport, byte* buf, uint len, void* param)
{
	debug_printf("收到：");
    Sys.ShowString(buf, len);
    debug_printf("\r\n");
	
	if(len > 4)
		return 0;
    
    return 0;
}

SerialPort* sp1;
void TestSerial()
{
    debug_printf("\r\n\r\n");
    debug_printf("TestSerial Start......\r\n");

    // 串口输入
    sp1 = new SerialPort(COM1);
    sp1->Open();
    sp1->Register(OnUsartRead);
    
	char str[] = "http://www.NewLifeX.com \r\n";
    sp1->Write((byte*)str, ArrayLength(str));
    //Sys.Sleep(3000);

    debug_printf("\r\nTestSerial Finish!\r\n");
}
