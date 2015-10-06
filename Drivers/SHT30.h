#ifndef _SHT30_H_
#define _SHT30_H_

#include "I2C.h"

// 光强传感器
class SHT30
{
public:
    I2C* IIC;		// I2C通信口
	byte Address;	// 设备地址

    SHT30();
    virtual ~SHT30();

	void Init();
	uint ReadSerialNumber();
	ushort ReadStatus();
	//ushort ReadTemperature();
	//ushort ReadHumidity();
	bool Read(ushort& temp, ushort& humi);

private:
	bool Write(ushort cmd);
	ushort Read2(ushort cmd);
	uint Read4(ushort cmd);	// 同时读取温湿度并校验Crc
};

/*
开发历史

2015-10-03
SHT30三种采集数据方式：
1，Stretch阻塞模式，发送命令后采集，需要长时间等待SCL拉高，才能发送读取头然后读取数据
2，Polling非阻塞模式，发送命令后采集，需要反复多次启动并发送读取头，得到ACK以后才能读取数据
3，内部定期采集模式，启动时发送Periodic命令，读取时发送FetchData命令后直接读取数据
感谢kazuyuki@407605899

*/

#endif
