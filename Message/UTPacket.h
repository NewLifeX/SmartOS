#ifndef __UTPacket_H__
#define __UTPacket_H__

#include "Sys.h"
#include "TokenNet\TokenMessage.h"
#include "Message\BinaryPair.h"
#include "Device\UTPort.h"
#include "TokenNet\TokenClient.h"


// unvarnished transmission 透传报文
// 由bsp注册端口到 Ports ID和对应的类
// C++ 没有反射  找不到由UTPort派生的子类。 
// 为了节省内存，UTPort只包含Port指针 Name指针 和一个虚函数  在没有create之前只占用12字节（3个指针）
class UTPacket : public Object
{
private:
	Dictionary<uint, UTPort*>	Ports;	// 端口集合   Dic不支持byte 所以用uint替代
	TokenClient * Client;				//
	uint AycUptTaskId;					// 异步上传数据ID
	MemoryStream * CacheA;				// 缓冲数据
	// MemoryStream * CacheB;			// 暂时不考虑双缓冲结构

public:
	UTPacket();
	UTPacket(TokenClient * client);
	~UTPacket();

	bool Set(TokenClient* client);

	bool Send(Buffer & packet);
	void AsynUpdata();
	bool Register(byte id,UTPort* port);
	bool PressTMsg(const BinaryPair& args, Stream& result);
	// Client.Register("UTPacket",&UTPacket::PressTMsg,this);

	static UTPacket * Current;
#if DEBUG
	virtual String& ToStr(String& str) const;
#endif

};

#endif
