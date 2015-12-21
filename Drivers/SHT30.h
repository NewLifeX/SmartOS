#ifndef _SHT30_H_
#define _SHT30_H_

#include "I2C.h"
#include "Power.h"

// 光强传感器
class SHT30 : public Power
{
public:
    I2C* IIC;		// I2C通信口
	byte Address;	// 设备地址
	byte Mode;		// 模式。0=CLKSTRETCH/1=POLLING/2=Periodic
	byte Frequency;	// 频率，多少秒测量一次。05/1/2/4/10，05表示0.5s
	byte Repeatability;	// 重复性。0=高/1=中/2=低，多次测量相差不多，说明重复性高

	OutputPort	Pwr;	// 电源

    SHT30();
    virtual ~SHT30();

	void Init();
	uint ReadSerialNumber() const;
	ushort ReadStatus() const;
	//ushort ReadTemperature();
	//ushort ReadHumidity();
	bool Read(ushort& temp, ushort& humi);

	// 电源等级变更（如进入低功耗模式）时调用
	virtual void ChangePower(int level);

private:
	bool Write(ushort cmd);
	ushort Read2(ushort cmd) const;
	// 同时读取温湿度并校验Crc
	uint Read4(ushort cmd) const;

	bool CheckStatus();
	void SetMode();
	ushort GetMode() const;
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
