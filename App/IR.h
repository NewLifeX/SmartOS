
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
public :
	typedef enum 
	{
		send = 0x01,
		receive = 0x02,
		idle	= 0x00,
	}IRMode;
	typedef enum
	{
		WaitSend,
		Sending,
		SendError,
		WaitRec,
		Recing,
		RecOutTime,
		RecError,
		Over,
	}IRStat;
	
private :
	PWM * _irf;
	OutputPort * _Outio;	// 发数据脚
	InputPort * _Inio;	// 接收管引脚
	
	volatile IRMode _mode;
	volatile IRStat _stat;
public:
	Timer * _timer;
private :
	volatile uint _timerTick;
	byte* _buff;
	int  _length;
	
public :
	IR(PWM * pwm ,Pin Outio,Pin Inio);
	
	bool SetMode(IRMode mode);
	IRMode GetMode(){return _mode;};
	IRStat GetStat(){return _stat;};
	
private :
	// 设置高低电平  38k 载波出去的高低电平		Start()函数耗时太长 不能再中断里面做  
	// pmos管驱动  低电平导通
	void SetIRH(){*_Outio=false;/*_irf->Start();*/}
	void SetIRL(){*_Outio=true; /*_irf->Stop ();*/}
	bool _nestOut;	// 下一个输出状态 
public :
	// 发送完成return true； 发送失败 return false；
	bool Send(byte *,int length);
	// return 长度   收发冲突 return 0   超时 return -1   出错 return -2  （接收到的数据过分短也是出错）
	int Receive(byte *);
	
	void static _timerHandler(void* sender, void* param);
	void SendReceHandler(void* sender, void* param);
};

