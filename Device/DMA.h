#ifndef __DMA_H__
#define __DMA_H__

// DMA
class DMA
{
private:
	byte _index;	// 第几个定时器，从0开始
	bool _started;

public:
	DMA(byte index);
	~DMA();

    int Retry;  // 等待重试次数，默认200
    int Error;  // 错误次数

	bool Start();	// 开始
	bool WaitForStop();	// 停止

	//typedef void (*TimerHandler)(Timer* tim, void* param);
	//void Register(TimerHandler handler, void* param = nullptr);

private:
	//static void OnHandler(ushort num, void* param);
	//TimerHandler _Handler;
	//void* _Param;
};

#endif
