#ifndef __UTPacket_H__
#define __UTPacket_H__

#include "Sys.h"
#include "TokenNet\TokenMessage.h"
#include "Message\BinaryPair.h"


enum  UTRet :int
{
	Good = 1,
	CmdError = 2,
	PortError =3,
	NoPort = 4,
	Error = -1,
};

// unvarnished transmission 透传端口基类
class UTPort
{
public:
	String * Name;		// 传输口名称
	byte	ID;
	virtual  UTRet DoFunc(Buffer & line, MemoryStream & ret) = 0;		// line 为输入命令，ret为返回值。

private:

};

// 放到其他地方去   不要放在此处。
class UTCom:public UTPort
{
public:
	// SerialPort * Port;
	virtual UTRet DoFunc(Buffer & line, MemoryStream & ret);
};


// unvarnished transmission 透传报文
class UTPacket
{
public:
	// List<UTPort*> Ports;	// 透传传输口集合
	

	bool PressTMsg(const BinaryPair& args, Stream& result);
	// Client.Register("UTPacket",&UTPacket::PressTMsg,this);

	//UTPort * CreateUTPort(byte portid);

	virtual void Show() const = 0;
};

#endif
