#ifndef __AnChe_H_
#define __AnChe_H_

#include "Kernel\Sys.h"
#include "Platform\Pin.h"
class AnCheConfig;

class AnChe
{
public :
	AnCheConfig* Config;
	void GetConfig();
};

// 配置
class AnCheConfig : public ConfigBase
{
public:
	byte	Anche;
	bool	Serial1Power;		// 串口2开关状态
	int		BaudRate1;			// 串口2波特率
	byte    DataBits1;			// 串口2数据位
	byte	Parity1;			// 串口2奇偶校验位
	byte	StopBits1;			// 串口2停止位
	bool	Serial2Power;		// 串口3开关状态
	int		BaudRate2;			// 串口3波特率
	byte    DataBits2;			// 串口3数据位
	byte	Parity2;			// 串口3奇偶校验位
	byte	StopBits2;			// 串口3停止位
	bool	IsInfrare;			// 是否红外控制
	ushort  InitWeight[2];		// 初始重量变化阈值
	ushort  StableWeight[2];	// 稳定重量变化阈值	
	byte	TagEnd;

	AnCheConfig();
};

#endif
