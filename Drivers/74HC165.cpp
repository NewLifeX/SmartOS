#include "74HC165.h"


IC74HC165::IC74HC165(Pin pl, Pin sck, Pin in, Pin ce) 
					: _PL(pl),_SCK(sck),_In(in)
{
	_PL.Invert = true;
	
	_PL.Open();
	_SCK.Open();
	_In.Open();
	
	if(ce != P0)
	{
		_CE = new OutputPort(ce);
		_CE.Invert = true;
		_CE.Open();
	}
}

bool IC74HC165::Read(byte *buf, byte count)
{
	if(!buf)return;
	_PL = false;	// 不采集
	if(_CE) _CE	= true;	// 使能芯片
	_PL = true;		// 采集
	_PL = false;	// 采集
	
	_SCK = false;
	
	for(int i = 0; i < count; i++)
	{
		byte temp;
		for(int j = 0; j < 8; j++)
		{
			temp <<= 1;
			_SCK = true;
			if(_In) temp |= 0x01;
			else	temp &= 0xfe;
			_SCK = false;
		}
		
		*buf = temp;
		buf++;
	}
	if(_CE) _CE = false;
	return true;
}

byte IC74HC165::Read()
{
	byte temp;
	Read(&temp, 1);
	return temp;
}
