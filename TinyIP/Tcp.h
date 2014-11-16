#ifndef _TinyIP_TCP_H_
#define _TinyIP_TCP_H_

#include "TinyIP.h"

// Tcp会话
class TcpSocket : public Socket
{
private:
	byte seqnum;

	TCP_HEADER* Create();
	
public:
	// Tcp状态
	typedef enum
	{
		Closed = 0,
		SynSent = 1,
		Established = 2,
	}TCP_STATUS;

	ushort		Port;		// 本地端口。0表示接收所有端口的数据包
	IPAddress	RemoteIP;	// 远程地址
	ushort		RemotePort;	// 远程端口
	IPAddress	LocalIP;	// 本地IP地址
	ushort		LocalPort;	// 本地端口
	TCP_STATUS	Status;		// 状态

	TCP_HEADER* Header;

	TcpSocket(TinyIP* tip);

	// 处理数据包
	virtual bool Process(MemoryStream* ms);

	bool Connect(IPAddress ip, ushort port);	// 连接远程服务器，记录远程服务器IP和端口，后续发送数据和关闭连接需要
    void Send(byte* buf, uint len);			// 向Socket发送数据，可能是外部数据包
    void Close();	// 关闭Socket
	void Ack(uint len);

	void Send(TCP_HEADER* tcp, uint len, byte flags);

	void Head(TCP_HEADER* tcp, uint ackNum, bool mss, bool cp_seq);

	// 收到Tcp数据时触发，传递结构体和负载数据长度。返回值指示是否向对方发送数据包
	typedef bool (*TcpHandler)(TcpSocket* socket, TCP_HEADER* tcp, byte* buf, uint len);
	TcpHandler OnAccepted;
	TcpHandler OnReceived;
	TcpHandler OnDisconnected;
};

#endif
