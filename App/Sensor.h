#ifndef __SENSOR_H__
#define __SENSOR_H__

#include "Kernel\Sys.h"
#include "Device\Port.h"


class Sensor
{
public:
	cstring Name;  //名字
	int Index;   //索引号

	
	OutputPort* Led;	// 指示灯
	OutputPort* Buzzer;	// 蜂鸣器
	
	InputPort* Key;	// 输入按键
	InputPort* Pir;	   //人体感应
	InputPort* Mag;	   //门磁
	
	//I2C*   Ifrared    //红外转发
	
	Delegate<Sensor&>	Press;

	// 构造函数。指示灯和继电器一般开漏输出，需要倒置
	Sensor() { Init(); }
	Sensor(Pin key, Pin led = P0, bool ledInvert = true, Pin buzzer = P0, bool keyInvert=true);
	Sensor(Pin key, Pin led = P0, Pin buzzer = P0);
	~Sensor();

	bool GetValue();
	void SetValue(bool value);

	//void Register(EventHandler handler, void* param = nullptr);

	
private:
	void Init();

	static void OnPress(InputPort* port, bool down, void* param);
	void OnPress(InputPort& port, bool down);

	//EventHandler _Handler;
	//void* _Param;
	
private:
	bool _Value; // 状态
};

#endif
