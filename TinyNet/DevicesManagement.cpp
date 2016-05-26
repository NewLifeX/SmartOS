#include "DevicesManagement.h"
#include "Message\BinaryPair.h"

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

int	DevicesManagement::PushDev(Device* dv)
{
	//DeviceRequest(DeviceAtions::Register, dv);
	int temp = DevArr.Push(dv);
	SaveDev();
	return temp; // DevArr.Push(dv);
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
	_Addr = addr;
	_FlashSize = size;
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
	if (hardid.Length() == 0)return nullptr;
	for (int i = 0; i < DevArr.Length(); i++)
	{
		// Buffer 不支持 == 判断两个对象是否相等  （只判断是否是同一个Arr内存地址）
		// if (DevArr[i] != nullptr&&hardid == DevArr[i]->HardID)return DevArr[i];
		bool isEqual = true;
		if (DevArr[i] != nullptr)
		{
			auto dv = DevArr[i];
			for (int j = 0; j < hardid.Length(); j++)
			{
				if (hardid[j] != dv->HardID[j])
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

	debug_printf("Load %d Dev from 0x%08X\r\n", i, cfg.Address);

	byte len = DevArr.Length();
	debug_printf("Devices has %d Dev\r\n", len);
	// 如果加载得到的设备数跟存入的设备数不想等，则需要覆盖一次
	if (len != i)SaveDev();

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

bool DevicesManagement::DeviceProcess(String &act, const Message& msg)
{
	TS("DevicesManagement::DeviceProcess");
	// 仅处理来自云端的请求
	if (msg.Reply) return false;

	DeviceAtions devact = DeviceAtions::Register;
	if (act == "Device/List")devact = DeviceAtions::List;
	if (act == "Device/Update")devact = DeviceAtions::Update;
	if (act == "Device/Delete")devact = DeviceAtions::Delete;
	if (act == "Device/ListIDs")devact = DeviceAtions::ListIDs;
	// if (act == "Device/Register")devact = DeviceAtions::Register;
	// if (act == "Device/Online")devact = DeviceAtions::Online;
	// if (act == "Device/Offline")devact = DeviceAtions::Offline;
	if (devact == DeviceAtions::Register)debug_printf("不支持的命令\r\n");

	auto ms = msg.ToStream();
	BinaryPair bp(ms);
	// 建立Reply
	auto tmsg = (*(TokenMessage*)&msg).CreateReply();

	switch (devact)
	{
	case DeviceAtions::List:
	{
		// 获取需要发送的IDs
		ByteArray idss;
		bp.Get("IDs", idss);
		// 准备写入器
		MemoryStream ms2;
		BinaryPair bp2(ms2);
		// 写入命令类型
		bp2.Set("Action", act);
		// 设备DevsInfo数据
		for (int i = 0; i < idss.Length(); i++)	// 判定依据需要修改
		{
			// 获取数据ms
			MemoryStream dvms;
			// 序列化一个DevInfo到ms
			if (!GetDevInfo(idss[i], dvms))continue;
			// 转换为ByteArray
			ByteArray dvbs(dvms.GetBuffer(), dvms.Position());
			String countstr;
			countstr += i;
			// 写入DevInfo
			bp2.Set(countstr, dvbs);
		}
		// 将所有数据写入Data
		tmsg.SetData(Buffer(ms2.GetBuffer(), ms2.Position()));
		// 发送
		//Port->Reply(tmsg);
	}
	break;
	case DeviceAtions::Delete:
	{
		// 拿到操作对象
		byte addr = 0;
		bp.Get("ID", addr);
		DeleteDev(addr);
		// 拿到操作对象s
		ByteArray idss;
		bp.Get("IDs", idss);

		for (int i = 0; i < idss.Length(); i++)
		{
			auto dv = FindDev(idss[i]);
			// 外部处理一下
			if (_DevProcess)_DevProcess(devact, dv, _ClbkParam);
			DeleteDev(idss[i]);
		}

		// 准备回复数据
		if (addr == 0x00 && idss.Length() == 0)
		{
			debug_printf("DeleteDev Error\r\n");
			tmsg.Error = true;
		}
	}
	break;
	case DeviceAtions::ListIDs:
	{
		MemoryStream ms2;
		BinaryPair bp2(ms2);

		bp2.Set("Action", act);
		//　获取IDs ByteArray
		MemoryStream idsms;
		WriteIDs(idsms);
		// for (int i = 0; i < DevArr.Length(); i++)
		// {
		// 	if (DevArr[i] == nullptr)continue;
		// 	idsms.Write(DevArr[i]->Address);
		// }
		ByteArray idbs(idsms.GetBuffer(), idsms.Position());
		// 写入名值对
		bp2.Set("IDs", idbs);

		tmsg.SetData(Buffer(ms2.GetBuffer(), ms2.Position()));
	}
	break;
	/*case DeviceAtions::Update:
		break;
	case DeviceAtions::Register:
		break;
	case DeviceAtions::Online:
		break;
	case DeviceAtions::Offline:
		break;*/
	default:
		debug_printf("不支持的设备操作指令！！\r\n");
		tmsg.Error = true;
		break;
	}
	return Port->Reply(tmsg);
}

// 获取设备信息到流
bool DevicesManagement::GetDevInfo(byte id, MemoryStream &ms)
{
	if (id == 0x00)return false;
	Device * dv = FindDev(id);
	return GetDevInfo(dv, ms);
}

// 获取设备信息到流
bool DevicesManagement::GetDevInfo(Device *dv, MemoryStream &ms)
{
	if (dv != nullptr)return false;

	BinaryPair bp(ms);

	MemoryStream dvms;
	BinaryPair dvbp(dvms);
	bp.Set("ID", dv->Address);

	byte login = dv->Logined ? 1 : 0;
	bp.Set("Online", login);
	bp.Set("Kind", dv->Kind);
	bp.Set("LastActive", dv->LastTime);
	bp.Set("RegisterTime", dv->RegTime);
	bp.Set("LoginTime", dv->LoginTime);
	// bp.Set("HardID", dv->Logins);

	bp.Set("Version", dv->Version);
	bp.Set("DataSize", dv->DataSize);
	bp.Set("ConfigSize", dv->ConfigSize);

	bp.Set("SleepTime", dv->SleepTime);
	bp.Set("Offline", dv->OfflineTime);
	bp.Set("PingTime", dv->PingTime);

	bp.Set("HardID", dv->HardID);
	bp.Set("Name", dv->Name);

	bp.Set("Password", dv->Pass);
	return true;
}

void DevicesManagement::SendDevicesIDs()
{
	// 获取IDList
	MemoryStream idms;
	WriteIDs(idms);
	ByteArray idbs(idms.GetBuffer(), idms.Position());

	TokenMessage msg(0x08);
	MemoryStream ms;
	BinaryPair bp(ms);
	bp.Set("Action", String("Device/ListIDs"));
	bp.Set("IDs", idbs);

	msg.SetData(Buffer(ms.GetBuffer(), ms.Position()));
	if (Port)Port->Send(msg);
}
// 设备状态上报  一次一个设备   未完待续
bool DevicesManagement::SendDevices(DeviceAtions act, const Device* dv)
{
	TS("DevicesManagement::SendDevices");
	// 不许为空
	if (dv == nullptr)return false;
	// 不存在主动发送这两条
	if (act == DeviceAtions::List || act == DeviceAtions::ListIDs)return false;
	// 保证可以顺利执行
	if (Port == nullptr)return false;
	if (Port->Status < 2) return false;
	// 只有 Register 需要发送完整设备信息
	String actstr;
	switch (act)
	{
	case DeviceAtions::Register:
		actstr = "Device/Register";
		break;
		// case DeviceAtions::Update:
		// 	actstr = "Device/Update";
		// 	break;
		// case DeviceAtions::Online:
		// 	actstr = "Device/Online";
		// 	break;
		// case DeviceAtions::Offline:
		// 	actstr = "Device/Offline";
		// 	break;
		// case DeviceAtions::Delete:
		// 	actstr = "Device/Delete";
		// 	break;
		// case DeviceAtions::ListIDs:
		// 	break;
		// case DeviceAtions::List:
		// 	break;
	default:
		debug_printf("无法处理的指令\r\n");
		return false;
		
	}
	if (actstr.Length() == 0)return false;

	MemoryStream datams;
	BinaryPair bp(datams);
	bp.Set("Action", actstr);
	GetDevInfo((Device*)dv, datams);

	TokenMessage tmsg(0x08);
	tmsg.SetData(Buffer(datams.GetBuffer(), datams.Position()));
	Port->Send(tmsg);
	return true;
}

void DevicesManagement::DeviceRequest(DeviceAtions act, byte id)
{
	if (id == 0x00)return;
	auto dv = FindDev(id);
	DeviceRequest(act, dv);
}
// 节点消息处理
void DevicesManagement::DeviceRequest(DeviceAtions act, const Device* dv)
{
	TS("DevicesManagement::DeviceRequest");

	bool PortOk = true;
	if (Port == nullptr)PortOk = false;
	if (Port->Status < 2) PortOk = false;
	if(Port == false)debug_printf("Port Not Realy\r\n");

	byte id = dv->Address;
	switch (act)
	{
	case DeviceAtions::List:
		return;
	case DeviceAtions::Update:
		return;
	case DeviceAtions::Online:
		debug_printf("节点上线 ID=0x%02X\r\n", id);
		break;
	case DeviceAtions::Offline:
		debug_printf("节点离线 ID=0x%02X\r\n", id);
		break;
	case DeviceAtions::Register:
		PushDev((Device*)dv);
		debug_printf("节点注册入网 ID=0x%02X\r\n", id);
		if(PortOk)SendDevices(act, dv);
		return;
	case DeviceAtions::Delete:
		debug_printf("节点删除~~ ID=0x%02X\r\n", id);
		DeleteDev(id);
		//if(PortOk)SendDevices(act, dv);
		break;
	default:
		debug_printf("无法识别的节点操作 Act=%d ID=0x%02X\r\n", (byte)act, id);
		break;
	}
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


