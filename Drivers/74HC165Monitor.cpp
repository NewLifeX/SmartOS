#include "74HC165Monitor.h"


IC74HC165MOR::IC74HC165MOR(Pin pl, Pin sck, Pin in, byte bufSize,bool highSpeed) 
					: _PL(pl),_SCK(sck),_In(in),_Bs(bufSize)
{
	_PL.Invert = true;
	_sckHighSpeed = highSpeed;
	Opened = false;
}

bool IC74HC165MOR::Open()
{
	if(Opened)return true;
	
	_PL.HardEvent = true;	// 硬中断
	_PL.Mode = InputPort::Rising;		// 上升沿
	//_PL.Register([](InputPort* port, bool down, void* param){ ((IC74HC165MOR*)param)->Trigger();},this);
	_PL.Press.Bind(&IC74HC165MOR::OnTrigger, this);
	_PL.UsePress();
	_PL.Open();
	
	if(!_sckHighSpeed)
	{
		_PL.HardEvent = true;	// 硬中断
		_SCK.Mode = InputPort::Rising;		// 上升沿
		//_SCK.Register([](InputPort* port, bool down, void* param){ ((IC74HC165MOR*)param)->ReaBit();},this);
		_SCK.Press.Bind(&IC74HC165MOR::OnReaBit, this);
		_SCK.UsePress();
	}
	_SCK.Open();
	_In.Open();
	Opened = true;
	return true;
}

bool IC74HC165MOR::Close()
{
	_PL.Close();
	_SCK.Close();
	_In.Close();	
	Opened = false;
	return true;
}

void IC74HC165MOR::OnTrigger(InputPort& port, bool down)
{
	if(down) Trigger();
}

void IC74HC165MOR::OnReaBit(InputPort& port, bool down)
{
	if(down) ReaBit();
}

void IC74HC165MOR::Trigger()
{
	if(!_sckHighSpeed)
	{
		_irqCount = 0;
		return;
	}
	//int bits = _Bs.Length() * 8;	// 总位数
	for(int i = 0; i < _Bs.Length(); i++)
	{
		byte temp = 0x00;
		int j	= 0;
		for(j = 0; j < 8; j++)
		{
			int times	= 2000;
			while(!_SCK && --times);	// 等待 高电平
			if(!times) break;
		
			if(_In) temp |= 0x01;
			else	temp &= 0xfe;
			
			times	= 2000;
			while(_SCK && --times);	// 等待低电平
			if(!times) break;
			
			temp <<= 1;
		}
		_Bs[i] = temp;
		//if(tw.Expired())return;	 // 2ms 不结束 就强制退出
		if(j < 8) break;
	}
}

void IC74HC165MOR::ReaBit()
{
	byte temp = _Bs[_irqCount/8];
	if(_In)
	{
		byte x = 0x01 << (_irqCount >> 3);
		temp |= x;
	}
	else
	{
		byte x = ~(0x01 << (_irqCount >> 3));
		temp &= x;
	}
	
	_Bs[ _irqCount >> 3 ] = temp;
	_irqCount++;
}

byte IC74HC165MOR::Read(byte *buf, byte count)
{
	Open();
	return _Bs.CopyTo(0, buf, count);
}

byte IC74HC165MOR::Read()
{
	Open();
	return *_Bs.GetBuffer();
}
