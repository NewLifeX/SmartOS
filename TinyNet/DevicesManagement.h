#ifndef __DevMgmt_H__
#define __DevMgmt_H__

#include "Sys.h"
#include "Stream.h"
#include "TokenNet\Device.h"
#include "TokenNet\DeviceMessage.h"

#include "Flash.h"
#include "TinyConfig.h"

#include "TokenNet\TokenClient.h"
#include "TokenNet\DeviceMessage.h"
#include "Security\Crc.h"

// DevicesManagement 服务于客户端/云端   所以内部 Port 限定为TokenClient
class DevicesManagement
{
public:
	DevicesManagement();
	~DevicesManagement();

	bool SetFlashCfg(uint addr, uint size);
	int Length() { return DevArr.Length(); }

	Device* FindDev(byte id)const;
	Device* FindDev(const Buffer& hardid) const;
	int		PushDev(Device* dv) { return DevArr.Push(dv); }
	bool	DeleteDev(byte id);

	int LoadDev();
	void SaveDev();
	void ClearDev();
	void ShowDev();

	int WriteIDs(Stream & ms);

	// 设备列表
	TArray<Device*> DevArr;
	// 持久在线列表
	TArray<Device*> OnlineAlways;

	static DevicesManagement * Current;

	// 发送时刻再绑定？！ 如果绑定失败报错？
	TokenClient * Port = nullptr;

	bool DeviceProcess(String &act, const Message& msg);
	bool GetDevInfo(byte id, MemoryStream &ms);
	bool GetDevInfo(Device *dv, MemoryStream &ms);
	bool SendDevices(DeviceAtions act, const Device* dv = nullptr);
	void SendDevicesIDs();
	// 设备状态变更上报
	void DeviceRequest(DeviceAtions act, const Device* dv = nullptr);
	// 维护设备状态
	void MaintainState();

	// token 控制设备列表的 外部处理注册回调
	typedef void(*DevPrsCallback)(DeviceAtions act, const Device* dv, void * param);
	void Register(DevPrsCallback func, void* param) { _DevProcess = func; _ClbkParam = param; };

private:
	const Config GetStore(Flash &flash);
	uint _Addr = 0;		// 固化到flash的地址
	uint _FlashSize = 0;

	DevPrsCallback  _DevProcess = nullptr;
	void * _ClbkParam = nullptr;
};


#endif // !__DevMgmt_H__
