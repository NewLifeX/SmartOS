#ifndef _TinyIP_HttpClient_H_
#define _TinyIP_HttpClient_H_

#include "TinyIP.h"

// Http客户端
class HttpClient : public TcpSocket
{
public:

	HttpClient(TinyIP* tip);

	// 处理数据包
	virtual bool Process(Stream* ms);

	bool Connect(IPAddress ip, ushort port);	// 连接远程服务器，记录远程服务器IP和端口，后续发送数据和关闭连接需要
    void Send(const byte* buf, uint len);			// 向Socket发送数据，可能是外部数据包
    void Disconnect();	// 关闭Socket

	// 收到Tcp数据时触发，传递结构体和负载数据长度。返回值指示是否向对方发送数据包
	typedef bool (*TcpHandler)(TcpSocket* socket, TCP_HEADER* tcp, byte* buf, uint len);
	TcpHandler OnAccepted;
	TcpHandler OnReceived;
	TcpHandler OnDisconnected;

	virtual cstring ToString();

protected:
	void SendAck(uint len);

	void SetSeqAck(TCP_HEADER* tcp, uint ackNum, bool cp_seq);
	void SetMss(TCP_HEADER* tcp);
	void Send(TCP_HEADER* tcp, uint len, byte flags);

	virtual void OnProcess(TCP_HEADER* tcp, Stream& ms);
	virtual void OnAccept(TCP_HEADER* tcp, uint len);
	virtual void Accepted2(TCP_HEADER* tcp, uint len);
	virtual void OnDataReceive(TCP_HEADER* tcp, uint len);
	virtual void OnDisconnect(TCP_HEADER* tcp, uint len);

	virtual bool OnOpen();
    virtual void OnClose();

    virtual bool OnWrite(const byte* buf, uint len);
	virtual uint OnRead(byte* buf, uint len);
};

#endif
