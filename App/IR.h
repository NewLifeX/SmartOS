
#ifndef __IR_H__
#define __IR_H__

#include "Timer.h"
#include "Port.h"

/*
	因为引脚跟定时器不是一一对应 还有 remap的关系   引脚不好处理需自行初始化引脚
保证能输出PWM
*/

/*
空调的发送数据是打包发送   一个数据包里面包含很多参数  一般长度比较长
电视机遥控器的数据相对比较短  但是喜欢一个按键下去发送几次

	经测试  空调的跳变沿大概有 200个   要预备好足够大的空间进行数据接收和发送

这里 将接收到的数据长度放到数据接收区的第一个字节   后面发送的时候需要注意
*/

class IR
{
private:
	PWM*	_Pwm			= nullptr;
	Timer*	_Tim			= nullptr;
	AlternatePort * _Port	= nullptr;
public:
	IR(PWM * pwm);
	
	bool Open();
	bool Close();

	bool Send(const Buffer& bs);
	int Receive(Buffer& bs, int sTimeout = 10);

private:
	static void OnSend(void* sender, void* param);
};

#endif
