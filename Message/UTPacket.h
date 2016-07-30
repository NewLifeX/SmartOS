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

	// 设置Client对象引用  顺带注册Invoke
	bool Set(TokenClient* client);
	// 发送   带缓冲区   packet>256Byte则直接发送 不进缓冲区
	bool Send(Buffer & packet);
	// 异步发送任务
	void AsynUpdata();
	// 添加UTPort成员
	bool AndPort(byte id,UTPort* port);
	// Invoke回调函数
	bool PressTMsg(const BinaryPair& args, Stream& result);

	static UTPacket * Current;
#if DEBUG
	virtual String& ToStr(String& str) const;
#endif

};

#endif
