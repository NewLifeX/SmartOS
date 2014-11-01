#include "Sys.h"
#include "NRF24L01.h"

#include "conf.h"

byte tx_buf[] = "It's 0123456789AB Time:00000000";

NRF24L01* nrf;

void OnSend(void* param)
{
	// 最后4个字节修改为秒数
    //uint s = __REV(Time.Current() / 1000000);
	// 大概4.86%的误差
    uint s = __REV(Time.Current() >> 20);
    byte* p = tx_buf + ArrayLength(tx_buf) - 8;
    Sys.ToHex(p, (byte*)&s, 4);

    //nrf->SetMode(false);
    if(!nrf->Write(tx_buf, ArrayLength(tx_buf)))
    {
        NRF24L01::RF_STATUS st;
        st.Init(nrf->Status);
        debug_printf ("Test Send Error 0x%02x\r\n", nrf->Status);
        if(!st.MAX_RT) nrf->ShowStatus();
    }
    //nrf->SetMode(true);
}

void OnReceive(void* param)
{
    byte buf[32];
    //nrf->SetMode(true);
	uint len = nrf->Read(buf, ArrayLength(buf));
    if(len)
    {
        Sys.ShowString(buf, len);
        debug_printf("\r\n");
    }
}

uint OnReceive(ITransport* transport, byte* buf, uint len, void* param)
{
	if(len)
	{
        Sys.ShowString(buf, len);
        debug_printf("\r\n");
	}
	return 0;
}

void TestNRF24L01()
{
    debug_printf("\r\n");
    debug_printf("TestNRF24L01 Start......\r\n");

    // 修改数据，加上系统ID
    byte* p = tx_buf + 5;
    Sys.ToHex(p, (byte*)Sys.ID, 6);

    nrf = Create2401();
    nrf->Timeout = 1000;
    nrf->Channel = 0;
    nrf->AutoAnswer = false;
    if(!nrf->Check())
        debug_printf("请检查线路\r\n");
    else
    {
        //nrf->Config();
        //nrf->SetMode(true);
        //Sys.AddTask(OnReceive, NULL, 0, 1000);
		nrf->Register(OnReceive, nrf);
        Sys.AddTask(OnSend, NULL, 0, 1000000);
    }

    debug_printf("TestNRF24L01 Finish!\r\n");
}
