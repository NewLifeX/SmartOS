#ifndef _AP0803_H_
#define _AP0803_H_

#include "Kernel\Sys.h"
#include "Net\ITransport.h"
#include "Net\Socket.h"

#include "App\Alarm.h"

// 阿波罗0803 GPRS通信
class AP0803
{
public:
	List<Pin>	LedPins;
	List<Pin>	ButtonPins;
	List<OutputPort*>	Leds;
	List<InputPort*>	Buttons;

	List<OutputPort*>	Outputs;
	List<InputPort*>	Inputs;

	Alarm*			AlarmObj;

	AP0803();

	// 设置系统参数
	void Init(ushort code, cstring name, COM message = COM1);

	// 设置数据区
	void* InitData(void* data, int size);
	// 写入数据区并上报
	void Write(uint offset, byte data);
	//获取客户端的状态0，未握手，1已握手，2已经登陆
	int GetStatus();
	
	typedef bool(*Handler)(uint offset, uint size, bool write);
	void Register(uint offset, uint size, Handler hook);
	void Register(uint offset, IDataPort& dp);

	void InitLeds();
	void InitButtons(const Delegate2<InputPort&, bool>& press);

	// 打开GPRS
	NetworkInterface* CreateGPRS();

	void InitClient();
	void InitNet();
	void InitProxy();
	void InitAlarm();
	void Restore();
	// invoke指令
	void Invoke(const String& ation, const Buffer& bs);
	void OnLongPress(InputPort* port, bool down);

	static AP0803* Current;

private:
	void*	Data;
	int		Size;
};

#endif
