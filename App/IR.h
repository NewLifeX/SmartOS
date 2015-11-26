
#include "Timer.h"
#include "Port.h"

/*
	因为引脚跟定时器不是一一对应 还有 remap的关系   引脚不好处理需自行初始化引脚
保证能输出PWM
*/

// 发送时钟使用100us 作为最小颗粒

/*
空调的发送数据是打包发送   一个数据包里面包含很多参数  一般长度比较长
电视机遥控器的数据相对比较短  但是喜欢一个按键下去发送几次

	经测试  空调的跳变沿大概有 200个   要预备好足够大的空间进行数据接收和发送

这里 将接收到的数据长度放到数据接收区的第一个字节   后面发送的时候需要注意
*/

class IR
{
private:
	PWM*	_Pwm;
	Timer*	_Tim;
	Array*	_Arr;
	short	_Index;
	short	_Ticks;
	bool	_Last;
	bool	_Mode;

public:
	OutputPort	Tx;	// 发数据
	InputPort	Rx;	// 接收
	bool		Opened;

	IR(PWM* pwm, Pin tx, Pin rx);

	bool Open();
	bool Close();

	bool Send(const Array& bs);
	int Receive(Array& bs);

private:
	void OnSend();
	void OnReceive();
};
