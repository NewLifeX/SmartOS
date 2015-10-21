#include "SerialPort.h"

uint OnUsartRead(ITransport* transport, ByteArray& bs, void* param, void* param2)
{
	debug_printf("收到：");
	bs.Show(true);
    
    return 0;
}

SerialPort* sp1;
void TestSerial()
{
    debug_printf("\r\n\r\n");
    debug_printf("TestSerial Start......\r\n");

    // 串口输入
    //sp1 = new SerialPort(COM1);
    sp1 = SerialPort::GetMessagePort();
#ifdef STM32F0
    sp1->Close();
	sp1->SetBaudRate(512000);
#endif
    sp1->Open();
    sp1->Register(OnUsartRead);
    
	char str[] = "http://www.NewLifeX.com \r\n";
    sp1->Write(ByteArray((byte*)str, ArrayLength(str)));
    //Sys.Sleep(3000);

    debug_printf("\r\nTestSerial Finish!\r\n");
}
