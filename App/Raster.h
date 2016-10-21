#ifndef __RASTER__H__
#define __RASTER__H__

#include "Sys.h"
#include "Message\BinaryPair.h"
#include "Device\Port.h"
#include "App\PulsePort.h"
#include "App\BlinkPort.h"

#include "Kernel\TTime.h"
#include"Board\AP0801.h"

#pragma pack(push)
#pragma pack(1)

// 光栅组数据部分
struct  RasTriData
{
	byte Index;				//光栅组
	byte  Direction;		//方向 
	UInt64  TriTime;		//触发时间
	//byte  Reserved;		//保留 （数据对齐）
	ushort Interval;		//AB响应间隔 单位MS

	ushort  OcclusionA;		//光栅A遮挡时间
	ushort  OcclusionB;		//光栅B遮挡时间
	ushort	Count;			// 本地计数

	byte Size() { return sizeof(this[0]); }
};
// 单路光栅部分
struct  FlagData
{
	byte	Count;			//计数 
	UInt64  TriTime;		//触发时间				
	ushort  Occlusion;		//光栅遮挡时间

	byte Size() { return sizeof(this[0]); }
};
#pragma pack(pop)

// 光栅类  直接对接 TokenClient
class Raster
{
public:
	AP0801*		AP;			// 外网客户端
	IDataPort*	Led;		// 指示灯

	Raster(Pin pinA, Pin pinB, Pin bz = P0);
	~Raster();

	bool Open();
	// 上报数据
	void Report();
	bool ReadyCache;
	void LineReport();

public:
	ushort	FlagTime;			//标志最长时常，超过时长统统清零,默认1500ms,
	byte	Index;				//光栅组索引
private:
	bool Opened;
	PulsePort*  RasterA;		//每一组光栅分两路
	PulsePort*  RasterB;

	uint	LastRTime;			// 最后一次上报时间
	uint	MinRTime;			// 最小发送频率，默认500ms
	uint    _task;
	//Buffer Cache;				//数据缓冲区
	MemoryStream	Cache;		//实际送的数据
private:
	FlagData FlagA;				//A标志
	FlagData FlagB;				//B标志
	byte Count;
	//光栅计数
	OutputPort	Buzzer;
	BlinkPort	BnkPort;
private:
	void Init(Pin bz);
	void OnHandlerA(PulsePort& raster);
	void OnHandlerB(PulsePort& raster);

	//定时清空标志
	void  RestFlag();
};


#endif
