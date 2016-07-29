#ifndef __UTPacket_H__
#define __UTPacket_H__

#include "Sys.h"
#include "TokenNet\TokenMessage.h"
#include "Message\BinaryPair.h"


enum  EorrCode : byte
{
	Good		= 1,
	CmdError	= 2,
	NoPort		= 3,
	Error		= 4,
};

typedef struct
{
	byte PortID;	// 端口号
	byte Seq;		// paket序列号 回复消息的时候原样返回不做校验
	byte Type;		// 数据类型
	EorrCode Error;	// 错误编码返回时候使用  （凑对齐）
	ushort Length;	// 数据长度
	void * Next()const { return (void*)(&Length + Length + sizeof(Length)); };
}PacketHead;

// unvarnished transmission 透传端口基类
class UTPort
{
public:
	String * Name;		// 传输口名称
	virtual  void DoFunc(Buffer & packet, MemoryStream & ret) = 0;		// packet 为输入命令，ret为返回值。

private:
};

// 放到其他地方去   不要放在此处。
class UTCom : public UTPort
{
public:
	// SerialPort * Port;
	virtual void DoFunc(Buffer & packet, MemoryStream & ret);
};

// unvarnished transmission 透传报文
// 由bsp注册端口到 Ports ID和对应的类
// C++ 没有反射  找不到由UTPort派生的子类。 
// 为了节省内存，UTPort只包含Port指针 Name指针 和一个虚函数  在没有create之前只占用12字节（3个指针）
class UTPacket : public Object
{
public:
	Dictionary<uint, UTPort*>	Ports;	// 端口集合   Dic不支持byte 所以用uint替代

	bool PressTMsg(const BinaryPair& args, Stream& result);
	// Client.Register("UTPacket",&UTPacket::PressTMsg,this);
#if DEBUG
	virtual String& ToStr(String& str) const;
#endif

};

#endif
