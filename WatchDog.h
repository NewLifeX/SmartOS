#ifndef __WatchDog_H__
#define __WatchDog_H__

#include "Sys.h"

// 看门狗
class WatchDog
{
private:

public:
	//WatchDog(uint ms = 3000);
	~WatchDog();

	uint Timeout; // 当前超时时间

	bool Config(uint ms);	// 配置看门狗喂狗重置时间，超过该时间讲重启MCU
	void ConfigMax();		// 看门狗无法关闭，只能设置一个最大值
	void Feed(); // 喂狗

	// 打开看门狗。最长喂狗时间26208ms，默认2000ms
	static void Start(uint ms = 2000);
};

#endif
