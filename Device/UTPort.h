#ifndef __UTPort_H__
#define __UTPort_H__

#include "Kernel\Sys.h"
#include "Device\SerialPort.h"
#include "Message/Pair.h"

enum  PacketEorrCode : byte
{
	Good = 1,
	CmdError = 2,	// 命令错误
	NoPort = 3,		// 没有这个端口
	NotOpen = 4,	// 没有打开
	CfgError = 5,	// 配置信息错误
	Error = 6,		// 未知错误
};

enum PacketFlag : byte
{
	Save = 1,		// 保存
	CycleDo = 1,	// 定时执行
};

typedef union 
{
	PacketEorrCode  ErrorCode;
	PacketFlag		Flag;
}PacketUnion;


typedef struct
{
	byte PortID;	// 端口号
	byte Seq;		// paket序列号 回复消息的时候原样返回不做校验 每个端口各自有一份
	byte Type;		// 命令ID/数据类型
	PacketUnion Error;	// 错误编码返回时候使用（凑对齐）  数据包标识云端下发命令事使用  平时为0
	ushort Length;	// 数据长度
	void * Next()const { return (void*)(&Length + Length + sizeof(Length)); };
}PacketHead;


// unvarnished transmission 透传端口基类
class UTPort
{
public:
	UTPort();
	byte	Seq;		// SEQ 值 用于辨认包的先后顺序，255 应该足够
	String * Name;		// 传输口名称
	virtual  void DoFunc(Buffer & packet, MemoryStream & ret) = 0;		// packet 为输入命令，ret为返回值。
private:
};

// 放到其他地方去   不要放在此处。
class UTCom : public UTPort
{
public:
	UTCom();
	byte State;		// 0 Port = null   1有端口    2已初始化   3已Open
	SerialPort * Port;
	virtual void DoFunc(Buffer & packet, MemoryStream & ret);

	void CreatePort() { Port = new SerialPort(); State = 1; };
	void DelPort() { if (Port)delete Port; State = 0; };

	bool ComConfig(Pair & data);
};


#endif
