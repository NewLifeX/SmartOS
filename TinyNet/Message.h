#ifndef __Message_H__
#define __Message_H__

#include "Sys.h"
#include "Net\ITransport.h"
#include "Stream.h"

// 消息
// 头部按照内存布局，但是数据和校验部分不是
// 测试指令 0201-0100-0000-51CC，从1发往2，功能1，标识0，校验0xCC51
class Message
{
public:
	// 标准头部，符合内存布局。注意凑够4字节，否则会有很头疼的对齐问题
	byte Dest;		// 目的地址
	byte Src;		// 源地址
	byte Code;		// 功能代码
	byte Flags:6;	// 标识位。也可以用来做二级命令
	byte Error:1;	// 是否错误
	byte Reply:1;	// 是否响应
	ushort Length;	// 数据长度
	ushort Checksum;// 16位检验和
	
	// 负载数据及校验部分，并非内存布局。
	ushort Crc16;	// 整个消息的Crc16校验，计算前Checksum清零
	byte* Data;		// 数据部分

public:
	// 初始化消息，各字段为0
	void Init();
	// 分析数据，转为消息。负载数据部分将指向数据区，外部不要提前释放内存
	bool Parse(MemoryStream& ms);
	// 验证消息校验和是否有效
	bool Verify();
};

#define MESSAGE_SIZE offsetof(Message, Checksum) + 2

class Controller
{
private:
	ITransport** _ports;	// 数据传输口
	int _portCount;
	ITransport* _curPort;	// 当前使用的数据传输口

	static uint OnReceive(ITransport* transport, byte* buf, uint len, void* param);

public:
	byte Address;	// 本地地址

	Controller(ITransport* port);
	Controller(ITransport* ports[], int count);
	~Controller();

	uint Send(byte dest, byte code, byte* buf = NULL, uint len = 0);
	// 发送消息
	bool Send(Message& msg);
	bool Send(Message& msg, ITransport* port);
	bool SendSync(Message& msg, uint msTimeout = 10);
	// 回复对方的请求消息
	bool Reply(Message& msg);
	bool Error(Message& msg);

protected:
	bool Process(MemoryStream& ms);

// 处理器部分
public:
	// 处理消息，返回是否成功
	typedef bool (*CommandHandler)(Message& msg, void* param);

private:
    struct CommandHandlerLookup
    {
        uint Code;
        CommandHandler Handler;
		void* Param;
    };
	CommandHandlerLookup* _Handlers[16];
	byte _HandlerCount;

public:
	// 注册消息处理器。考虑到业务情况，不需要取消注册
	void Register(byte code, CommandHandler handler, void* param = NULL);

// 常用系统级消息
public:
	static bool SysTime(Message& msg, void* param);
	static bool SysID(Message& msg, void* param);
	static bool Discover(Message& msg, void* param);

// 测试部分
public:
	static void Test(ITransport* port);
};

/*class DiscoverMessage : public Message
{
public:
	//const
}*/

#endif
