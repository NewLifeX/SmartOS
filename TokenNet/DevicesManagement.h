#ifndef __DevMgmt_H__
#define __DevMgmt_H__

#include "Sys.h"
#include "Device.h"

#include "Flash.h"
#include "TinyNet\TinyConfig.h"

#include "TokenClient.h"
#include "DeviceMessage.h"
#include "Security\Crc.h"

// DevicesManagement 服务于客户端/云端   所以内部 Port 限定为TokenClient
class DevicesManagement
{
public:
	// 全局只允许一个设备管理器  Invoke 也就使用这个
	static DevicesManagement * Current;
	static bool InvokeList		(void * param, const BinaryPair& args, Stream& result);
	static bool InvokeUpdate	(void * param, const BinaryPair& args, Stream& result);
	static bool InvokeDelete	(void * param, const BinaryPair& args, Stream& result);
	static bool InvokeListIDs	(void * param, const BinaryPair& args, Stream& result);
							 
public:
	DevicesManagement();
	~DevicesManagement();

	bool SetFlashCfg(uint addr, uint size);
	int Length() { return DevArr.Count(); }

	Device* FindDev(byte id)const;
	Device* FindDev(const Buffer& hardid) const;

	int LoadDev();
	void ClearDev();
	void ShowDev();

private:	
	// 外部操作使用 DeviceRequest
	void SaveDev();
	bool DeleteDev(byte id);
	int	PushDev(Device* dv);
public:
	// 设备列表
	IList	DevArr;
	// 持久在线列表
	// List	OnlineAlways;

	TokenClient * Port = nullptr;
	// 发送时刻再绑定？！ 如果绑定失败报错？
	void SetTokenClient(TokenClient *port);

	// 处理TokenMsg的设备指令
	//bool DeviceProcess(String &act, const Message& msg);
	bool DeviceProcess(DeviceAtions act,const BinaryPair& args, Stream& result);
	void SendDevicesIDs();
	// 设备状态变更上报 由TinyServer这种对象调用
	void DeviceRequest(DeviceAtions act, const Device* dv = nullptr);
	void DeviceRequest(DeviceAtions act, byte id);
private:
	// 把ids写入到ms
	int WriteIDs(Stream & ms);
	// 获取设备信息到 MemoryStream (名值对形式)
	bool GetDevInfo(byte id, MemoryStream &ms);
	bool GetDevInfo(Device *dv, MemoryStream &ms);
	// 发送单个设备信息  只在维护设备状态时使用
	bool SendDevices(DeviceAtions act, const Device* dv = nullptr);
public:
	// 维护设备状态
	void MaintainState();

	typedef void(*DevPrsCallback)(DeviceAtions act, const Device* dv, void * param);
	// 注册到使用 TokenMsg Delete 设备的时候
	void Register(DevPrsCallback func, void* param) { _DevProcess = func; _ClbkParam = param; };

private:
	const Config GetStore(Flash &flash);
	uint _Addr = 0;		// 固化到flash的地址
	uint _FlashSize = 0;

	DevPrsCallback  _DevProcess = nullptr;
	void * _ClbkParam = nullptr;
};


#endif // !__DevMgmt_H__
