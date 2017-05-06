#ifndef _IOK0612_H_
#define _IOK0612_H_

#include "B8266.h"

// WIFI WRGB调色
class IOK0612 : public B8266
{
public:
	byte	LedsShow;					// LED 显示状态开关  0 刚启动时候的20秒   1 使能   2 失能
	uint	LedsTaskId;

	IOK0612();
	void InitLeds();
	void FlushLed();			// 刷新led状态输出
	byte LedStat(byte showmode);

	static IOK0612* Current;
};

#endif
