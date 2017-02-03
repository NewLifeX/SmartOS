#ifndef _Pandora_H_
#define _Pandora_H_

#include "Sys.h"
#include "Net\ITransport.h"

#include "Device\RTC.h"
#include "TokenNet\TokenClient.h"
#include "Message\ProxyFactory.h"
#include "App\Alarm.h"

// 潘多拉0903
class PA0903
{
public:
	List<Pin>	LedPins;
	List<OutputPort*>	Leds;

	//List<Pin>	ButtonPins;
	//List<InputPort*>	Buttons;

	List<OutputPort*>	Outputs;
	List<InputPort*>	Inputs;

	ISocketHost*	Host;	// 网络主机
	//ISocketHost*	HostAP;	// 网络主机
	TokenClient*	Client;	// 令牌客户端
	ProxyFactory*	ProxyFac;	// 透传管理器
	Alarm*			AlarmObj;
	
	PA0903();

	// 设置系统参数
	void Init(ushort code, cstring name, COM message = COM2);

	// 设置数据区
	void* InitData(void* data, int size);
	void Register(int index, IDataPort& dp);

	void InitLeds();
	//void InitButtons();
	//void InitPort();

	// 打开以太网W5500
	ISocketHost* Create5500();

	// 打开Esp8266，作为主控或者纯AP
	ISocketHost* Create8266(bool apOnly);
	//ITransport* Create2401();

	void InitClient();
	void InitNet();
	void InitProxy();
	void InitAlarm();

	void Restore();
	//void OnLongPress(InputPort* port, bool down);

	static PA0903* Current;

private:
	void*	Data;
	int		Size;

	void OpenClient(ISocketHost& host);
	TokenController* AddControl(ISocketHost& host, const NetUri& uri, ushort localPort);
};

#endif
