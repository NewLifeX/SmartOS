#ifndef _TinyIP_TCP_H_
#define _TinyIP_TCP_H_

#include "W5500.h"

// Tcp会话
class TcpSocket : public HardwareSocket, public ITransport
{
public:
	// Tcp状态
	typedef enum
	{
		Closed = 0,
		SynSent = 1,
		SynAck = 2,
		Established = 3,
	}TCP_STATUS;

	ushort 		Port;		// 本地端口，接收该端口数据包。0表示接收所有端口的数据包
	ushort		BindPort;	// 绑定端口，用于发出数据包的源端口。默认为Port，若Port为0，则从1024算起，累加
	IPEndPoint	Remote;		// 远程地址。默认发送数据的目标地址
	IPEndPoint	Local;		// 本地地址

	TCP_STATUS	Status;		// 状态

	TCP_HEADER* Header;

	TcpSocket(W5500* pw5500):HardwareSocket(pw5500);

	// 处理数据包
	virtual bool Process(IP_HEADER& ip, Stream& ms);

	bool Connect(IPAddress& ip, ushort port);	// 连接远程服务器，记录远程服务器IP和端口，后续发送数据和关闭连接需要
    bool Send(ByteArray& bs);			// 向Socket发送数据，可能是外部数据包
    bool Disconnect();	// 关闭Socket

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
	bool Send(TCP_HEADER& tcp, uint len, byte flags);

	virtual void OnProcess(TCP_HEADER& tcp, Stream& ms);
	virtual void OnAccept(TCP_HEADER& tcp, uint len);
	virtual void OnAccept3(TCP_HEADER& tcp, uint len);
	virtual void OnDataReceive(TCP_HEADER& tcp, uint len);
	virtual void OnDisconnect(TCP_HEADER& tcp, uint len);

	virtual bool OnOpen();
    virtual void OnClose();

    virtual bool OnWrite(const byte* buf, uint len);
	virtual uint OnRead(byte* buf, uint len);
};

#endif
