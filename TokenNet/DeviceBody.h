#ifndef __DeviceBody_H__
#define __DeviceBody_H__

#include "Sys.h"
#include "Message\DataStore.h"

//#include "TokenNet\TokenClient.h"

#include "TinyNet\Device.h"
#include "TinyNet\DevicesManagement.h"

// tokendistributive


// 令牌网
class DeviceBody
{
private:
	bool Opened = false;
	// 第二数据区起始地址
	uint St2BaseAddr;
public:
	Device DevInfo;
	DataStore Store;

	// 第二数据区 
	DataStore * Store2;

	DeviceBody();
	// 设置第二数据区
	bool SetSecDaStor(uint addr,byte* buf,int len);
	bool SetSecDaStor(DataStore * store);

	// 获取数据区指针
	// byte* GetData();

	void Open();
	virtual bool OnOpen() = 0;
	void Close();
	virtual bool OnClose() = 0;

	// 消息收发器 对其进行读写
	void OnWrite(const TokenMessage& msg);
	void OnRead(const TokenMessage& msg);

	// 直接找分发器的发送函数就好 
	// bool Send(TokenMassge &msg);
	// bool Reply(TokenMassge &msg);

	// 主动上报数据区数据    注意，这里 offset 为虚拟地址  特别小心 DOF（xx） 
	bool Report(uint offset, byte dat);
	bool Report(uint offset, const Buffer& bs);
};

#endif
