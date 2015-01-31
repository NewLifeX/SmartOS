#include "ADC.h"

void TestADC()
{
    debug_printf("\r\n\r\n");
    debug_printf("TestADC Start......\r\n");

    ADConverter adc(1, 0xFFFF);
    //ADConverter adc(1, 0x30000);
    //ADConverter adc(1, 0x300);
	adc.Open();

	while(true)
	{
		for(int i=0; i<18; i++)
			debug_printf("ADC[%d]=%d ", i+1, adc.Data[i]);
		debug_printf("\r\n");

		Sys.Sleep(1000);
	}

    debug_printf("\r\nTestADC Finish!\r\n");
}
