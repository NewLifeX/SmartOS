#include "DeviceMessage.h"

#include "Security\MD5.h"
#include "Message\BinaryPair.h"

// 初始化消息，各字段为0
DeviceMessage::DeviceMessage()
{
}

// 初始化消息，各字段为0
DeviceMessage::DeviceMessage(Device* dv)
{
	pDev = dv;
}

// 拿ID 不修改ms
bool DeviceMessage::GetBaseInfo(Stream& ms)
{
	BinaryPair bp(ms);
	byte act;
	if (!bp.Get("Action",act))return false;
	Action = (DeviceAtions)act;
	if (!bp.Get("ID", Id))return false;
	return true;
}

bool DeviceMessage::GetMsgInfo(Stream&ms, Device* dv)
{
	pDev = dv;
	byte id = 0;

	BinaryPair bp(ms);
	bp.Get("ID", id);
	if (pDev->Address == 0)pDev->Address = id;
	if (id != pDev->Address)
	{
		debug_printf("ERROR");
		return false;
	}
	byte login = 0;
	if (bp.Get("Online", login))
		pDev->Logined = login;
	bp.Get("Kind", pDev->Kind);
	bp.Get("LastActive", pDev->LastTime);
	bp.Get("RegisterTime", pDev->RegTime);
	bp.Get("LoginTime", pDev->LoginTime);
	// bp.Get("HardID", pDev->Logins);

	bp.Get("Version", pDev->Version);
	bp.Get("DataSize", pDev->DataSize);
	bp.Get("ConfigSize", pDev->ConfigSize);

	bp.Get("SleepTime", pDev->SleepTime);
	bp.Get("Offline", pDev->OfflineTime);
	bp.Get("PingTime", pDev->PingTime);

	bp.Get("HardID", pDev->HardID);
	bp.Get("Name", pDev->Name);

	bp.Get("Password", pDev->Pass);
	return true;
}

// 从数据流中读取消息
bool DeviceMessage::Read(Stream& ms)
{
	GetBaseInfo(ms);
	if (pDev == 0)return false;
	return GetMsgInfo(ms, pDev);
}

// 把消息写入数据流中
void DeviceMessage::Write(Stream& ms) const
{
	BinaryPair bp(ms);
	byte act = (byte)Action;
	bp.Set("Action", act);
	bp.Set("ID", pDev->Address);

	byte login = pDev->Logined ? 1 : 0;
	bp.Set("Online", login);
	bp.Set("Kind", pDev->Kind);
	bp.Set("LastActive", pDev->LastTime);
	bp.Set("RegisterTime", pDev->RegTime);
	bp.Set("LoginTime", pDev->LoginTime);
	// bp.Set("HardID", pDev->Logins);

	bp.Set("Version", pDev->Version);
	bp.Set("DataSize", pDev->DataSize);
	bp.Set("ConfigSize", pDev->ConfigSize);

	bp.Set("SleepTime", pDev->SleepTime);
	bp.Set("Offline", pDev->OfflineTime);
	bp.Set("PingTime", pDev->PingTime);

	bp.Set("HardID", pDev->HardID);
	bp.Set("Name", pDev->Name);

	bp.Set("Password", pDev->Pass);
}

#if DEBUG
// 显示消息内容
String& DeviceMessage::ToStr(String& str) const
{
	str += "DeviceMessage";

	return str;
}
#endif
