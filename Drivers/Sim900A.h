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
	COM_Def		Com;

	const char*	APN;

	IDataPort*	Led;	// 指示灯

    Sim900A();
    virtual ~Sim900A();

	bool Send(const Array& bs);

private:
	void Init(uint sTimeout = 3);

	virtual bool OnOpen();
    virtual void OnClose();

    virtual bool OnWrite(const Array& bs);
	virtual uint OnRead(Array& bs);

	String Send(const char* str, uint sTimeout = 3);
	bool SendCmd(const char* str, uint sTimeout = 3);
	void SendAPN(bool issgp);
};

#endif
