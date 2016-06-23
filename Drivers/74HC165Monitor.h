#ifndef _74HC165MOR_H_
#define _74HC165MOR_H_

#include "Port.h"
#include "Power.h"

/* 备注
调用构造函数后 需要Open()一下
如果不Open()  在Read 里面也会Open  但第一次读取不对
*/
class IC74HC165MOR
{
private:
	InputPort	_PL;
	InputPort	_SCK;
	InputPort	_In;
	
	ByteArray _Bs;		// 内部缓冲区
	byte 	_irqCount;	// 记录pl信号后第几位数据
	bool	_sckHighSpeed;
public:
	bool Opened;
	// pl采集，cp时钟，Q7输出， bufsize 是级联个数，highSpeed 是否是高速clk ms以下算高速
	IC74HC165MOR(Pin pl, Pin sck, Pin in, byte bufSize = 0,bool highSpeed = true);
	
	bool Open();
	bool Close();
	
	// 内部中断委托函数
	void Trigger();
	void ReaBit();
	
	// 级联读取  buf,数据放置位置，count，级联ic数
	byte Read(byte *buf, byte count);
	// 读一个IC 的数据
	byte Read();
};

#endif
