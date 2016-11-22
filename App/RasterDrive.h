#ifndef __RASTERONE__H__
#define __RASTERONE__H__

#include "TinyNet\Tiny.h"
#include "Sys.h"
#include "Device\Port.h"
#include "App\PulsePort.h"
#include "App\BlinkPort.h"

// 单路光栅配置信息
struct RasPin
{
	Pin 	ras0;	// 第一引脚
	Pin 	ras1;	// 第二引脚
	byte 	Idx;	// 编号
};

class RasterOne
{

public:
	uint Min = 0;		// 最小时间间隔 单位 ms 
	uint Max = 0;		// 最大时间间隔 单位 ms 
	UInt64  Last;		// 上一个信号触发时间
	UInt64  TriTime;			//触发时间
	UInt64  TriCount;			//触发次数
	ushort  WaveLength;			//波长
	void*  Param = nullptr;

public:
	// PulsePort 事件函数形式
	typedef void(*RasterOneHandler)(RasterOne* raster, void* param);
	RasterOne();
	RasterOne(Pin pin0, Pin bz = P0);
	~RasterOne();

	void Init(Pin pin);
	void Buzzer_Init(Pin pin);
	// Set Thresholds 设置阈值
	void SetThld(uint minTriIter, uint minTriTime);

	bool Open();
	void Close();

	RasterOneHandler	Handler = nullptr;
	void OnHandler(PulsePort& port);
	// 注册回调函数
	void Register(RasterOneHandler handler = NULL, void* param = NULL);

private:
	// 脉冲口
	PulsePort* Port;
	bool Opened = false;		// 是否开启
	OutputPort	Buzzer;
	BlinkPort	BnkPort;
private:
	bool 	IsOneLine = true;
	uint	_task = 0;
	void InputTask();
	//void Moni();
};

#endif
