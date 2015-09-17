#ifndef _TinyIP_TCP_H_
#define _TinyIP_TCP_H_

#include "TinyIP.h"

// Tcp会话
class TcpSocket : public TinySocket, public ITransport, public ISocket
{
private:
	uint		Seq;		// 序列号，本地发出数据包
	uint		Ack;		// 确认号，对方发送数据包的序列号+1

public:
	// Tcp状态
	typedef enum
	{
		Closed = 0,
		SynSent = 1,
		SynAck = 2,
		Established = 3,
	}TCP_STATUS;

	//ushort 		Port;		// 本地端口，接收该端口数据包。0表示接收所有端口的数据包
	//ushort		BindPort;	// 绑定端口，用于发出数据包的源端口。默认为Port，若Port为0，则从1024算起，累加
	//IPEndPoint	Remote;		// 远程地址。默认发送数据的目标地址
	//IPEndPoint	Local;		// 本地地址

	TCP_STATUS	Status;		// 状态

	TCP_HEADER* Header;

	TcpSocket(TinyIP* tip);

	// 处理数据包
	virtual bool Process(IP_HEADER& ip, Stream& ms);

	// 连接远程服务器，记录远程服务器IP和端口，后续发送数据和关闭连接需要
	bool Connect(IPAddress& ip, ushort port);
    bool Disconnect();	// 关闭Socket

	// 发送数据
	virtual bool Send(const ByteArray& bs);
	// 接收数据
	virtual uint Receive(ByteArray& bs);

	// 收到Tcp数据时触发，传递结构体和负载数据长度。返回值指示是否向对方发送数据包
	typedef bool (*TcpHandler)(TcpSocket& socket, TCP_HEADER& tcp, byte* buf, uint len);
	TcpHandler OnAccepted;
	TcpHandler OnReceived;
	TcpHandler OnDisconnected;

	virtual string ToString();

protected:
	void SendAck(uint len);

	void SetSeqAck(TCP_HEADER& tcp, uint ackNum, bool cp_seq);
	void SetMss(TCP_HEADER& tcp);
	bool SendPacket(TCP_HEADER& tcp, uint len, byte flags);

	virtual void OnProcess(TCP_HEADER& tcp, Stream& ms);
	virtual void OnAccept(TCP_HEADER& tcp, uint len);
	virtual void OnAccept3(TCP_HEADER& tcp, uint len);
	virtual void OnDataReceive(TCP_HEADER& tcp, uint len);
	virtual void OnDisconnect(TCP_HEADER& tcp, uint len);

	virtual bool OnOpen();
    virtual void OnClose();

    virtual bool OnWrite(const ByteArray& bs);
	virtual uint OnRead(ByteArray& bs);
};

#endif
