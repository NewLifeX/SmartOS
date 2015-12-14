#ifndef __ShunCom_H__
#define __ShunCom_H__

#include "Sys.h"
#include "Port.h"
#include "Power.h"
#include "Net\ITransport.h"
#include "Message\DataStore.h"

// 上海顺舟Zigbee协议
// 主站发送所有从站收到，从站发送只有主站收到
class ShunCom : public PackPort, public Power
{
private:

public:
	OutputPort	Reset;	// 复位
	IDataPort*	Led;	// 指示灯

	OutputPort	Power;	// 电源
	OutputPort	Sleep;	// 睡眠
	OutputPort	Config;	// 配置

	InputPort	Run;	// 组网成功后闪烁
	InputPort	Net;	// 组网中闪烁
	InputPort	Alarm;	// 警告错误

	ShunCom();

	void Init(ITransport* port, Pin rst = P0);

	void ShowConfig();

	// 电源等级变更（如进入低功耗模式）时调用
	virtual void ChangePower(int level);
	//设置设备诶类型：00代表中心、01代表路由，02代表终端
	virtual void SetDeviceMode(byte kind);
	//设置无线频点，注意大小端，Zibeer是小端存储
	virtual void SetChannel(int kind);
	//设置发送模式00为广播、01为主从模式、02为点对点模式
	virtual void SetSendMode(byte mode);	
	//进入配置PanID,同一网络PanID必须相同
    virtual void SetPanID(int ID);
	//进入配置模式
	virtual bool EnterSetMode();
	//退出配置模式
	virtual void OutSetMode();
	
	//读取配置信息
	virtual void ConfigMessage(ByteArray& buf);
	
    virtual bool OnWrite(const Array& bs);
	// 引发数据到达事件
	virtual uint OnReceive(Array& bs, void* param);

	virtual string ToString() { return "ShunCom"; }
	virtual bool OnOpen();

protected:
	
    virtual void OnClose();
};
class ShunComMessage
{
public:
	byte 		Frame;		//帧头
	byte		Length;		//数据长度
	uint		Code;		//操作码
	uint		CodeKind;  	//操类型
	uint		DataLength;	//负载数据长度
	byte   		Data[64];	// 负载数据部分
	byte		Checksum;	//异或校验、从数据长度到负载数据尾
public:
	//ShunComMessage(uint code,uint codeKind);
	virtual bool Read(Stream& ms);
	// 写入指定数据流
	virtual void Write(Stream& ms) const;
	// 验证消息校验码是否有效
	virtual bool Valid();
	// 显示消息内容
	virtual void Show() const;
};
#endif
