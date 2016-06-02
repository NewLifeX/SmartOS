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
	
	cstring	Domain;

	cstring	APN;

	IDataPort*	Led;	// 指示灯

    Sim900A();
    virtual ~Sim900A();

	bool Send(const Buffer& bs);

	//virtual const String ToString() const { return String("Sim900A"); }

private:
	void Init(uint msTimeout = 1000);

	virtual bool OnOpen();
    virtual void OnClose();

    virtual bool OnWrite(const Buffer& bs);
	virtual uint OnRead(Buffer& bs);

	String Send(cstring str, uint msTimeout = 1000);
	bool SendCmd(cstring str, uint msTimeout = 1000, int times = 1);
	void SendAPN(bool issgp);
	void SendDomain();
};

#endif
