#include "AP0803.h"

AP0803* AP0803::Current = nullptr;

AP0803::AP0803()
{
	LedPins.Add(PC7);		//PE5 0803上的网络指示灯
	LedPins.Add(PE4);
	LedPins.Add(PD0);
	ButtonPins.Add(PE9);
	ButtonPins.Add(PE14);

	Gsm.Com = COM4;
	Gsm.Baudrate = 115200;
	Gsm.Power = PE0;		// 0A04（170509）板上电源为PA4
	Gsm.Reset = PD3;
	Gsm.LowPower = P0;

	Current = this;
}

/*

GPRS(COM2)
PA4                 POWER
PE0					PWR_KEY
PD3					RST
PC10(TX4)		RXD
PC11(RX4)		TXD

PE9					KEY1
PE14				KEY2

PE5					LED1
PE4					LED2
PD0					LED3


USB
PA11 				_N
PA12				_P
*/
