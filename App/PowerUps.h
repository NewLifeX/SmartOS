
#ifndef __Music_H__
#define __Music_H__

#include "Sys.h"
#include "../Core/Delegate.h"


/*
	每次启动，FLASH 记录重启数加1。延迟5秒清零。
	如果没有清零，表示 timems 秒内有第二次重启。
	当有多次重启后可以判断。某段时间内
*/


class PowerUps
{
public:	
	// thld 重启次数阈值  timeMs 重启超时  act 重启次数超过阈值的动作
	PowerUps(byte thld,int timeMs = 5, Func act = nullptr);

	byte ReStartCount;	// 当前重启的次数
	byte ReStThld;		// 重启阈值，超过某个值则处理某事

	Func Act;			// 当重启次数超出阈值时执行的动作
	//uint taskid;		// 

	static void DelayAct(void *param);	// 延迟动作
};

#endif 
