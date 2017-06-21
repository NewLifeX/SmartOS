#ifndef __AT_H__
#define __AT_H__

// GPRS的AT指令集 GSM 07.07
class AT
{
public:
	ITransport*	Port;	// 传输口
	

	cstring	DataKey;	// 数据关键字

	Delegate<Buffer&>	Received;

	AT();
	~AT();

	void Init(COM idx, int baudrate = 115200);
	void Init(ITransport* port);
	float GetLatitude();
	float GetLongitude();

	// 打开与关闭
	bool Open();
	void Close();

	/******************************** 发送指令 ********************************/
	// 发送指令，在超时时间内等待返回期望字符串，然后返回内容
	String Send(const String& cmd, cstring expect, cstring expect2 = nullptr, uint msTimeout = 1000, bool trim = true);
	String Send(const String& cmd, uint msTimeout = 1000, bool trim = true);
	// 发送命令，自动检测并加上\r\n，等待响应OK
	bool SendCmd(const String& cmd, uint msTimeout = 1000);
	// 等待命令返回
	bool WaitForCmd(cstring expect, uint msTimeout);

private:
	void*		_Expect;	// 等待内容

	// 分析关键字。返回被用掉的字节数
	uint ParseReply(const Buffer& bs);

	// 引发数据到达事件
	uint OnReceive(Buffer& bs, void* param);
	static uint OnPortReceive(ITransport* sender, Buffer& bs, void* param, void* param2);
};

#endif
