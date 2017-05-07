#ifndef _Sim900A_H_
#define _Sim900A_H_

#include "GSM07.h"

// GPRS模块
class Sim900A : public GSM07
{
public:
    Sim900A();

	virtual bool IPSend(int index, const Buffer& data);

protected:
	virtual bool OnOpen();

	// 数据到达
	virtual void OnReceive(Buffer& bs);

private:
	//void Init(uint msTimeout = 1000);
};

#endif
