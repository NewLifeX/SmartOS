#ifndef _RX8025T_H
#define _RX8025T_H

#include "Kernel\Sys.h"
#include "I2C.h"
#include "Core\DateTime.h"

class RX8025T
{
	public:
		SoftI2C* IIC; // I2C通信口
		byte Address; // 设备地址
	public:
		RX8025T(byte devaddr = 0xA0, int wnus = 0);
		void Init();
		byte CheckDevice(void);//
		byte Output1s_Start(void);
		byte Output1s_Stop(void);
		int LoadTime(DateTime &dt);
		int SaveTime(DateTime &dt);		
	private:
		int RegWrite(byte sadd, byte  len);
		int RegRead(byte sadd, byte len);
	private:
		byte buf[20];
		byte devaddr;//设备地址
};

#endif
