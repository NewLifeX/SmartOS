#ifndef _TinyIP_TCP_H_
#define _TinyIP_TCP_H_

#include "TinyIP.h"

// Tcp会话
class TcpSocket : public Socket, public ITransport
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

	ushort 		Port;		// 本地端口，接收该端口数据包。0表示接收所有端口的数据包
	ushort		BindPort;	// 绑定端口，用于发出数据包的源端口。默认为Port，若Port为0，则从1024算起，累加
	IPAddress	RemoteIP;	// 远程地址
	ushort		RemotePort;	// 远程端口
	IPAddress	LocalIP;	// 本地IP地址
	ushort		LocalPort;	// 本地端口，收到数据包的目的端口
	TCP_STATUS	Status;		// 状态

	TCP_HEADER* Header;

	TcpSocket(TinyIP* tip);

	// 处理数据包
	virtual bool Process(IP_HEADER& ip, Stream& ms);

	bool Connect(IPAddress& ip, ushort port);	// 连接远程服务器，记录远程服务器IP和端口，后续发送数据和关闭连接需要
    void Send(ByteArray& bs);			// 向Socket发送数据，可能是外部数据包
    void Disconnect();	// 关闭Socket

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
	void Send(TCP_HEADER& tcp, uint len, byte flags);

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

// Tcp客户端
/*class TcpClient : public TcpSocket
{
public:

	TcpClient(TinyIP* tip);
};*/

#endif
