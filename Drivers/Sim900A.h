#ifndef _Sim900A_H_
#define _Sim900A_H_

#include "Net\ITransport.h"
#include "Message\DataStore.h"

// GPRS模块
class Sim900A : public ITransport
{
public:
    ITransport*	Port;
	int			Speed;
	COM			Com;
	bool		Inited;	
	bool		SendOK;
	
	const char*	Domain;

	const char*	APN;

	IDataPort*	Led;	// 指示灯

    Sim900A();
    virtual ~Sim900A();

	bool Send(const Array& bs);

private:
	void Init(uint msTimeout = 1000);

	virtual bool OnOpen();
    virtual void OnClose();

    virtual bool OnWrite(const Array& bs);
	virtual uint OnRead(Array& bs);

	String Send(const char* str, uint msTimeout = 1000);
	bool SendCmd(const char* str, uint msTimeout = 1000, int times = 1);
	void SendAPN(bool issgp);
	void SendDomain();
};

#endif
