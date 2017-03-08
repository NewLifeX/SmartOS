#ifndef __GSM07_H__
#define __GSM07_H__

#include "Device\Port.h"

#include "App\AT.h"

#include "Net\Socket.h"
#include "Net\NetworkInterface.h"
#include "Net\ITransport.h"

#include "Message\DataStore.h"
#include "Message\Pair.h"

// GPRS的AT指令集 GSM 07.07
class GSM07 : public NetworkInterface
{
public:
	AT		At;	// AT操作对象
	cstring	APN;

	IDataPort*	Led;	// 指示灯

	OutputPort	_Power;
	OutputPort	_Reset;
	OutputPort	_LowPower;

	Delegate<Buffer&>	Received;

	GSM07();
	virtual ~GSM07();

	void Init(ITransport* port);
    void Init(COM idx, int baudrate = 115200);
	void Set(Pin power = P0, Pin rst = P0);

	virtual void Config();
	void SetLed(Pin led);
	void SetLed(OutputPort& led);
	void RemoveLed();

	virtual Socket* CreateSocket(NetType type);

/******************************** 基础AT指令 ********************************/
	bool Test();
	bool Reset(bool soft);
	String GetVersion();
	bool Sleep(uint ms);
	bool Echo(bool open);
	// 恢复出厂设置，将擦写所有保存到Flash的参数，恢复为默认参数。会导致模块重启
	bool Restore();
	void SetAPN(cstring apn, bool issgp);

/******************************** 功能指令 ********************************/
	IPAddress GetIP(bool sta);

/******************************** TCP/IP ********************************/

/******************************** 发送指令 ********************************/

private:
	IPEndPoint	_Remote;	// 当前数据包远程地址

	// 打开与关闭
	virtual bool OnOpen();
	virtual void OnClose();
	// 检测连接
	virtual bool OnLink(uint retry);

	bool CheckReady();

	// 多个硬件socket
	int* _sockets[5];

	// 分析+IPD接收数据。返回被用掉的字节数
	uint ParseReceive(const Buffer& bs);
	// 分析关键字。返回被用掉的字节数
	uint ParseReply(const Buffer& bs);

	// 数据到达
	void OnReceive(Buffer& bs);
};

#endif
