#include "Drivers\NRF24L01.h"

extern NRF24L01* Create2401();

//const char tx_buf[] = "It's 0123456789AB Time:";

NRF24L01* nrf;

void OnSend(void* param)
{
	// 最后4个字节修改为秒数
	// 大概4.86%的误差
    //uint s = _REV(Sys.Ms() >> 10);
	String str("It's ");
	//str.SetLength(str.Length() - 8);
	//str.Append(s, 16, 8);
	str += Buffer(Sys.ID, 12);
	str += " Time:";
	str += Sys.Seconds();

    //nrf->SetMode(false);
    if(!nrf->Write(str.GetBytes()))
    {
        debug_printf ("Test Send Error 0x%02x\r\n", nrf->Status);
        nrf->ShowStatus();
    }
    //nrf->SetMode(true);
}

void OnReceive(void* param)
{
    ByteArray bs;
	uint len = nrf->Read(bs);
    if(len)
    {
		bs.Show(true);
    }
}

uint OnReceive(ITransport* transport, Buffer& bs, void* param, void* param2)
{
	bs.Show(true);

	return 0;
}

void TestNRF24L01()
{
    debug_printf("\r\n");
    debug_printf("TestNRF24L01 Start......\r\n");

    // 修改数据，加上系统ID
    //byte* p = tx_buf + 5;
    //Sys.ToHex(p, (byte*)Sys.ID, 6);
	//String str(tx_buf);
	//str.SetLength(5);
	//str.Append(ByteArray(Sys.ID, 6));

    nrf = Create2401();
    //nrf->Timeout = 1000;
    nrf->Channel = 0;
    nrf->AutoAnswer = false;
    if(!nrf->Check())
        debug_printf("请检查线路\r\n");
    else
    {
        //nrf->Config();
        //nrf->SetMode(true);
        //Sys.AddTask(OnReceive, nullptr, 0, 1);
		//nrf->Register(OnReceive, nrf);
        Sys.AddTask(OnSend, nullptr, 0, 1000);
    }

    debug_printf("TestNRF24L01 Finish!\r\n");
}
