#ifndef __Message_H__
#define __Message_H__

#include "Sys.h"
#include "ITransport.h"

// 消息
// 头部按照内存布局，但是数据和校验部分不是
class Message
{
private:
	// 标准头部，符合内存布局。注意凑够4字节，否则会有很头疼的对齐问题
	byte Dest;		// 目的地址
	byte Src;		// 源地址
	byte Code;		// 代码
	byte Reply:1;	// 是否响应
	byte Error:1;	// 是否错误
	byte Flags:6;	// 标识位

	// 负载数据及校验部分，并非内存布局。
	ushort Checksum;// 16位检验和
	ushort Length;	// 数据长度
	byte* Data;		// 数据部分

public:
	// 初始化消息，各字段为0
	void Init();
	// 分析数据，转为消息。负载数据部分将指向数据区，外部不要提前释放内存
	void Parse(byte* buf, uint len);
	// 验证消息校验和是否有效
	bool Verify();
	
public:	// 静态部分
	//static 
};

class Controller
{
private:
	ITransport* _port;	// 数据传输口

	void OnReceive(ITransport* transport, byte* buf, uint len, void* param);

public:
	Controller(ITransport* port);
	
protected:
	void Process(byte* buf, uint len);
}

class DiscoverMessage : public Message
{
public:
	//const 
}

#endif
