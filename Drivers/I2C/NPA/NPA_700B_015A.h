#ifndef _NPA_H
#define _NPA_H

#include "I2C.h"
//压力传感器 NPA-700B-015A
class NPA_700B_015A
{
public:
	SoftI2C* IIC;

	NPA_700B_015A();
	int Read();
	float ReadP();//读取大气压值
	void Init();
private:	
	byte Address; //设备地址
};
#endif
