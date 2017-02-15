#include "Kernel\Sys.h"
#include "Device\ADC.h"

void TestADC()
{
    debug_printf("\r\n\r\n");
    debug_printf("TestADC Start......\r\n");

    //ADConverter adc(1, 0xFFFF);
    //ADConverter adc(1, 0x30000);
    //ADConverter adc(1, (1 << 8) | (1 << 9));
	ADConverter adc(1, 0x30000);
	/*adc.Add(PB1);
	adc.Add(PB0);
	adc.Add(PC5);
	adc.Add(PC4);
	adc.Remove(PB0);

	adc.Add(PC3);
	adc.Add(PC2);
	adc.Add(PC1);
	adc.Add(PC0);

	adc.Add(PA6);
	adc.Add(PA7);
	adc.Add(PA1);
	adc.Add(PA0);

	adc.Add(PB0);*/

	adc.Open();

	for(int i=0; i<100; i++)
	{
		/*for(int i=0; i<adc.Count; i++)
			debug_printf("ADC[%d]=%d ", i+1, adc.Data[i]);
		debug_printf("\r\n");*/
		debug_printf("PB0=%d PB1=%d PA7=%d Temp=%d Verf=%d\r\n", adc.Read(PB0), adc.Read(PB1), adc.Read(PA7), adc.ReadTempSensor(), adc.ReadVrefint());

		Sys.Sleep(1000);
	}

    debug_printf("\r\nTestADC Finish!\r\n");
}
