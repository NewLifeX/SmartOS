#include "Kernel\Sys.h"
#include "ADC.h"

//Pin _Pins[] = ADC1_PINS;

ADConverter::ADConverter(byte line, uint channel)
{
	assert(line >= 1 && line <= 3, "ADC Line");

	Line	= line;
	Channel	= channel;

	uint dat	= 1;
	Count	= 0;
	for(int i=0; i<ArrayLength(Data); i++, dat <<= 1)
	{
		if(Channel & dat) Count++;
	}

	Buffer(Data, sizeof(Data)).Clear();

	_PinCount	= 0;
	_ADC		= nullptr;
	
	OnInit();
}

void ADConverter::Add(Pin pin)
{
	for(int i=0; i<_PinCount; i++)
	{
		if(_Pins[i] == pin)
		{
			Channel |= (1 << i);
			Count++;
			return;
		}
	}
}

void ADConverter::Remove(Pin pin)
{
	for(int i=0; i<_PinCount; i++)
	{
		if(_Pins[i] == pin)
		{
			Channel &= ~(1 << i);
			Count--;
			return;
		}
	}
}

void ADConverter::Open()
{
	debug_printf("ADC::Open %d 共%d个通道\r\n", Line, Count);

	uint dat = 1;
	for(int i=0; i<ArrayLength(Data); i++, dat <<= 1)
	{
		if(Channel & dat)
		{
			debug_printf("ADC::Init %d ", i+1);
			if(i < _PinCount)
				_ports[i].Set(_Pins[i]).Open();
			else
				debug_printf("\r\n");
		}
	}

	OnOpen();
}

ushort ADConverter::Read(Pin pin)
{
	ushort dat = 1;
	int n = 0;
	for(int i=0; i<_PinCount; i++, dat <<= 1)
	{
		if(Channel & dat)
		{
			n++;
			if(_Pins[i] == pin)
			{
				return Data[n-1];
			}
		}
	}
	return 0;
}
