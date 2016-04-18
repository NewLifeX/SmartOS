#include "DevicesManagement.h"


DevicesManagement* DevicesManagement::Current = nullptr;

DevicesManagement::DevicesManagement()
{
	DevArr.SetLength(0);
	OnlineAlways.SetLength(0);
	Current = this;
}

DevicesManagement::~DevicesManagement()
{
	// for (int i = 0; i < Length(); i++)
	// {
	// 	auto dv = DevArr[i];
	// 	if (!dv)continue;
	// 	delete dv;
	// }
	DevArr.SetLength(0);
	OnlineAlways.SetLength(0);
	Current = nullptr;
}

bool DevicesManagement::SetFlashCfg(uint addr, uint size)
{
	const uint minAddr = 0x8000000 + (Sys.FlashSize << 10) - (64 << 10);
	const uint maxAddr = 0x8000000 + (Sys.FlashSize << 10);
	if (addr<minAddr || addr>maxAddr)
	{
		debug_printf("设置的地址不在正确的位置上\r\n");
		return false;
	}
	const uint minSize = 4 << 10;
	const uint maxSize = 16 << 10;
	if (size<minSize || size>maxSize)
	{
		debug_printf("设置的大小不合适\r\n");
		return false;
	}
	_Addr 		= addr;
	_FlashSize 	= size;
	return true;
}

Device * DevicesManagement::FindDev(byte id) const
{
	if (id == 0)return nullptr;
	for (int i = 0; i < DevArr.Length(); i++)
		if (id == DevArr[i]->Address)return DevArr[i];
	return nullptr;
}

Device * DevicesManagement::FindDev(const Buffer & hardid) const
{
	if (hardid.Length())return nullptr;
	for (int i = 0; i < DevArr.Length(); i++)
	{
		// Buffer 不支持 == 判断两个对象是否相等  （只判断是否是同一个Arr内存地址）
		// if (DevArr[i] != nullptr&&hardid == DevArr[i]->HardID)return DevArr[i];
		bool isEqual = true;
		if (DevArr[i] != nullptr)
		{
			for (int i = 0; i < hardid.Length(); i++)
			{
				if (hardid[i] != DevArr[i]->HardID[i])
				{
					isEqual = false;
					break;
				}
			}
		}
		if (isEqual)return DevArr[i];
	}
	return nullptr;
}

bool DevicesManagement::DeleteDev(byte id)
{
	TS("DevicesManagement::DeleteDevice");

	auto	dv = FindDev(id);
	if (dv&&dv->Address == id)
	{
		debug_printf("DevicesManagement::DeleteDev Del id: 0x%02X\r\n", id);

		int idx = DevArr.FindIndex(dv);
		debug_printf("idx~~~~~~~~~~~:%d\r\n", idx);
		if (idx >= 0)DevArr[idx] = nullptr;

		// 处理持久在线表
		int idx2 = OnlineAlways.FindIndex(dv);
		if (idx2 >= 0)OnlineAlways[idx2] = nullptr;

		delete dv;
		SaveDev();

		return true;
	}

	return false;
}

const Config DevicesManagement::GetStore(Flash & flash)
{
	if (_Addr == 0)
	{
		//debug_printf("还未设置设备储存区地址\r\n");
		//debug_printf("使用默认最后4K空间设置\r\n");
		const uint addr = 0x8000000 + (Sys.FlashSize << 10) - (4 << 10);
		SetFlashCfg(addr, 4 << 10);
	}
	Config cfg(flash, _Addr, _FlashSize);
	return cfg;
}

int DevicesManagement::LoadDev()
{
	TS("DevicesManagement::Load");

	debug_printf("DevicesManagement::Load Dev\r\n");
	Flash flash;
	auto cfg = GetStore(flash);

	byte* data = (byte*)cfg.Get("Devs");
	if (!data) return -1;

	Stream ms(data, _FlashSize);
	// 设备个数
	auto count = ms.ReadByte();
	debug_printf("\tCount %d\r\n", count);
	int i = 0;
	for (; i < count; i++)
	{
		//debug_printf("\t加载设备:");

		bool fs = false;
		/*ms.Seek(1);
		byte id = ms.Peek();
		ms.Seek(-1);*/
		// 前面一个字节是长度，第二个字节才是ID
		byte id = ms.GetBuffer()[1];
		auto dv = FindDev(id);
		if (!dv)
		{
			dv = new Device();
			fs = true;
		}
		dv->Read(ms);
		dv->Show();

		if (fs)
		{
			int idx = DevArr.FindIndex(nullptr);
			if (idx == -1)
			{
				if (dv->Valid())
					DevArr.Push(dv);
				else
					delete dv;
				//debug_printf("\t Push");
			}
		}
		debug_printf("\r\n");
	}

	debug_printf("Load %d Dev from 0x%08X\r\n",i, cfg.Address);

	byte len = DevArr.Length();
	debug_printf("Devices has %d Dev\r\n", len);

	return i;
}

void DevicesManagement::SaveDev()
{
	TS("DevicesManagement::Save");

	Flash flash;
	auto cfg = GetStore(flash);

	byte buf[0x800];

	MemoryStream ms(buf, ArrayLength(buf));
	byte num = 0;
	for (int i = 0; i < DevArr.Length(); i++)
	{
		auto dv = DevArr[i];
		if (dv == nullptr) continue;
		num++;
	}

	// 设备个数
	//int count = num;
	debug_printf("\tCount %d\r\n", num);
	ms.Write((byte)num);

	for (int i = 0; i < DevArr.Length(); i++)
	{
		auto dv = DevArr[i];
		if (dv == nullptr) continue;
		dv->Write(ms);
	}
	debug_printf("DevicesManagement::SaveDevices Save %d Dev To 0x%08X！\r\n", num, cfg.Address);
	cfg.Set("Devs", Buffer(ms.GetBuffer(), ms.Position()));
}

void DevicesManagement::ClearDev()
{
	TS("DevicesManagement::ClearDev");

	Flash flash;
	auto cfg = GetStore(flash);

	debug_printf("DevicesManagement::ClearDevices Clear List 0x%08X \r\n", cfg.Address);

	for (int i = 0; i < DevArr.Length(); i++)
	{
		if (DevArr[i])delete DevArr[i];
		DevArr[i] = nullptr;
	}
	DevArr.SetLength(0);	// 清零后需要保存一下，否则重启后 Length 可能不是 0。做到以防万一
	SaveDev();
}

// 输出所有设备
void DevicesManagement::ShowDev()
{
	TS("DevicesManagement::DeviceShow");

	byte len = Length();
	byte count = 0;

	for (int i = 0; i < len; i++)
	{
		auto dv = DevArr[i];
		if (dv == nullptr) continue;

		count++;
		dv->Show();
		debug_printf("\r\n");

		//Sys.Sleep(0);
	}
	debug_printf("\r\nHas %d Dev\r\n", count);
	debug_printf("\r\n\r\n");
}

int DevicesManagement::WriteIDs(Stream &ms)
{
	int len = 0;
	for (int i = 0; i < DevArr.Length(); i++)
	{
		if (DevArr[i] == nullptr)continue;
		ms.Write(DevArr[i]->Address);
		len++;
	}
	return len;
}

bool DevicesManagement::DeviceProcess(const Message& msg)
{
	// 仅处理来自云端的请求
	if (msg.Reply) return false;

	TS("DevicesManagement::DeviceProcess");

	auto act = (DeviceAtions)msg.Data[0];
	byte id = msg.Data[1];

	TokenMessage rs;
	rs.Code = 0x21;
	rs.Length = 2;
	rs.Data[0] = (byte)act;
	rs.Data[1] = id;

	auto dv = FindDev(id);
	// 外部处理
	if (_DevProcess)_DevProcess(act, dv, _ClbkParam);

	switch (act)
	{
	case DeviceAtions::List:
	{
		SendDevices(act, nullptr);
		return true;
	}
	case DeviceAtions::Update:
	{
		// 云端要更新网关设备信息
		if (!dv)
		{
			rs.Error = true;
		}
		else
		{
			auto ms = msg.ToStream();
			ms.Seek(2);

			dv->ReadMessage(ms);
			SaveDev();
		}

		if(Port)Port->Reply(rs);
	}
	break;
	case DeviceAtions::Register:
		break;
	case DeviceAtions::Online:
		break;
	case DeviceAtions::Offline:
		break;
	case DeviceAtions::Delete:
		debug_printf("节~~1点删除 ID=0x%02X\r\n", id);
		{
			//auto dv2 = FindDev(id);
			if (dv == nullptr)
			{
				rs.Error = true;
				if (Port)Port->Reply(rs);
				return false;
			}

			// 云端要删除本地设备信息
			bool flag = DeleteDev(id);
			rs.Error = !flag;
			if (Port)Port->Reply(rs);
		}
		break;
	default:
		debug_printf("无法识别的节点操作 Act=%d ID=0x%02X\r\n", (byte)act, id);
		break;
	}

	return true;
}

// 设备列表 0x21
bool DevicesManagement::SendDevices(DeviceAtions act, const Device* dv)
{
	TS("DevicesManagement::SendDevices");

	if (Port)return false;
	if (Port->Status < 2) return false;

	TokenMessage msg;
	msg.Code = 0x21;

	int count = 0;
	int len = Length();

	for (int i = 0; i < len; i++)
	{
		if (DevArr[i] == nullptr) continue;
		count++;
	}

	if (dv) count = 1;

	byte buf[1500];		// 1024 字节只能承载 23条数据，udp最大能承载1514字节
	MemoryStream ms(buf, ArrayLength(buf));
	//MemoryStream ms(1536);
	ms.Write((byte)act);
	ms.Write((byte)count);

	if (len > 0)
	{
		if (dv)
			dv->WriteMessage(ms);
		else
		{
			for (int i = 0; i<len; i++)
			{
				auto dv1 = DevArr[i];
				if (dv1 == nullptr) continue;
				dv1->WriteMessage(ms);
			}
		}
	}

	msg.Length = ms.Position();
	msg.Data = ms.GetBuffer();

#if DEBUG
	switch (act)
	{
	case DeviceAtions::List:
		debug_printf("发送设备列表 共%d个\r\n", count);
		break;
	case DeviceAtions::Online:
		debug_printf("节点上线 ID=0x%02X \t", dv->Address);
		break;
	case DeviceAtions::Offline:
		debug_printf("节点下线 ID=0x%02X \t", dv->Address);
		break;
	default: break;
	}
#endif
	if (act == DeviceAtions::List)
		return Port->Reply(msg);
	else
		return Port->Send(msg);
}

void DevicesManagement::SendDevicesIDs()
{
	TokenMessage msg;
	msg.Code = 0x21;
	auto act = DeviceAtions::ListIDs;

	MemoryStream ms;
	ms.Write((byte)act);
	WriteIDs(ms);

	msg.Length = ms.Position();
	msg.Data = ms.GetBuffer();

	if (Port)Port->Send(msg);
}

// 节点消息处理 0x21
void DevicesManagement::DeviceRequest(DeviceAtions act, const Device* dv)
{
	TS("DevicesManagement::DeviceRequest");

	if (Port)return;
	if (Port->Status < 2) return;

	byte id = dv->Address;
	switch (act)
	{
	case DeviceAtions::List:
		SendDevices(act, dv);
		return;
	case DeviceAtions::Update:
		SendDevices(act, dv);
		return;
	case DeviceAtions::Register:
		debug_printf("节点注册入网 ID=0x%02X\r\n", id);
		SendDevices(act, dv);
		return;
	case DeviceAtions::Online:
		debug_printf("节点上线 ID=0x%02X\r\n", id);
		break;
	case DeviceAtions::Offline:
		debug_printf("节点离线 ID=0x%02X\r\n", id);
		break;
	case DeviceAtions::Delete:
	{
		debug_printf("节点删除~~ ID=0x%02X\r\n", id);
		auto dv = FindDev(id);
		if (dv == nullptr) return;
		DeleteDev(id);
		break;
	}
	default:
		debug_printf("无法识别的节点操作 Act=%d ID=0x%02X\r\n", (byte)act, id);
		break;
	}

	TokenMessage rs;
	rs.Code = 0x21;
	rs.Length = 2;
	rs.Data[0] = (byte)act;
	rs.Data[1] = id;

	Port->Send(rs);
}

void DevicesManagement::MaintainState()
{
	if (Port)return;
	if (Port->Status < 2) return;
	SendDevicesIDs();

	auto now = Sys.Seconds();

	// 处理持久在线设备
	for (int i = 0; i < OnlineAlways.Length(); i++)
	{
		auto dv = OnlineAlways[i];
		if (!dv)continue;
		dv->LastTime = now;
	}

	byte len = Length();
	for (int i = 0; i < len; i++)
	{
		auto dv = DevArr[i];
		if (!dv) continue;

		ushort time = dv->OfflineTime ? dv->OfflineTime : 60;

		// 特殊处理网关自身
		// if (dv->Address == gw->Server->Cfg->Address) dv->LastTime = now;

		if (dv->LastTime + time < now)
		{	// 下线
			if (dv->Logined)
			{
				//debug_printf("设备最后活跃时间：%d,系统当前时间:%d,离线阈值:%d\r\n",dv->LastTime,now,time);
				DeviceRequest(DeviceAtions::Offline, dv);
				dv->Logined = false;
			}
		}
		else
		{	// 上线
			if (!dv->Logined)
			{
				DeviceRequest(DeviceAtions::Online, dv);
				dv->Logined = true;
			}
		}
	}
}
