#include "Time.h"
#include "Device.h"

/******************************** Device ********************************/

Device::Device() :
	HardID(_HardID, sizeof(_HardID)),
	Mac(_Mac, sizeof(_Mac)),
	Name(_Name, sizeof(_Name)),
	Pass(_Pass, sizeof(_Pass)),
	Store(_Store, sizeof(_Store))
{
	Address		= 0;
	Logined		= false;

	Kind		= 0;
	LastTime	= 0;
	RegTime		= 0;
	LoginTime	= 0;
	Logins		= 0;

	Version		= 0;
	DataSize	= 0;
	ConfigSize	= 0;

	PingTime	= 0;
	OfflineTime	= 0;
	SleepTime	= 0;

	HardID.Clear();
	Mac.Clear();
	Name.Clear();
	Pass.Clear();
	Store.Clear();

	Cfg			= nullptr;

	LastRead	= 0;
	//LastWrite	= 0;
}

void Device::Write(Stream& ms) const
{
	TS("Device::Write");

/*#pragma  GCC diagnostic ignored  "-Winvalid-offsetof"
	Buffer bs((void*)&Address, offsetof(Device, Cfg) - offsetof(Device, Address));
#pragma  GCC diagnostic warning  "-Winvalid-offsetof"*/
	Buffer bs((void*)&Address, (byte*)&Cfg - (byte*)&Address);
	ms.WriteArray(bs);
}

void Device::Read(Stream& ms)
{
	TS("Device::Read");

	// 为了避免不同版本的配置兼容，指定长度避免覆盖过头
	//Buffer bs(&Address, offsetof(Device, Cfg) - offsetof(Device, Address));
	Buffer bs(&Address, (byte*)&Cfg - (byte*)&Address);
	//bs	= ms.ReadArray();
	// 为了减少一个临时对象，直接传入外部包装给内部拷贝
	uint len 	= ms.ReadEncodeInt();
	uint p		= ms.Position();
	ms.Read(bs);
	ms.SetPosition(p + len);
}

void Device::WriteMessage(Stream& ms) const
{
	byte* buf	= ms.Current();
	// 先写入0占位，后面回来写入大小
	ms.Write((byte)0);
	uint p		= ms.Position();

	ms.Write(Address);
	ms.Write(Kind);
	ms.WriteArray(HardID);
	ms.Write(LastTime);

	ms.Write(Version);
	ms.Write(DataSize);
	ms.Write(ConfigSize);

	ms.Write(SleepTime);
	ms.Write(OfflineTime);
	ms.Write(PingTime);

	ms.WriteArray(Name);

	// 计算并设置大小
	byte size	= ms.Position() - p;
	buf[0]		= size;
}

void Device::ReadMessage(Stream& ms)
{
	byte size	= ms.ReadByte();
	uint p		= ms.Position();

	Address	= ms.ReadByte();
	Kind	= ms.ReadUInt16();
	HardID	= ms.ReadArray();
	LastTime= ms.ReadUInt32();

	Version		= ms.ReadUInt16();
	DataSize	= ms.ReadByte();
	ConfigSize	= ms.ReadByte();

	SleepTime	= ms.ReadUInt16();
	OfflineTime	= ms.ReadUInt16();
	PingTime	= ms.ReadUInt16();

	Name	= ms.ReadString();

	// 最后位置
	ms.SetPosition(p + size);
}

bool Device::Valid() const
{
	if(Address 	== 0x00)	return false;
	if(Kind		== 0x0000)	return false;
	//if(HardID.Length() == 0)return false;

	return true;
}

#if DEBUG
String& Device::ToStr(String& str) const
{
	str += "Addr=0x";
	str.Concat(Address, -16);
	str += " Kind=";
	str.Concat(Kind, -16);
	str += " Hard=";
	str.Concat(HardID[0], -16);
	str.Concat(HardID[1], -16);
	str = str + " Mac=" + Mac;

	DateTime dt;
	dt.Parse(LastTime);
	str = str + " Last=" + dt;

	// 主数据区
	byte len	= Store[0];
	if(len > 1 && len <= 32)
	{
		str = str + " ";
		ByteArray(Store.GetBuffer(), len).ToStr(str);
	}

	//len	= strlen(Name);
	if(Name.Length() > 0)
	{
		str += "\t";
		//String(Name, len).ToStr(str);
		str	+= Name;
	}
	return str;
}
#endif

bool operator==(const Device& d1, const Device& d2)
{
	return d1.Address == d2.Address;
}

bool operator!=(const Device& d1, const Device& d2)
{
	return d1.Address != d2.Address;
}

/********************************************************************/

bool DevicesManagement::SetFlashCfg(uint addr,uint size)
{
	const uint minAddr = 0x8000000 + (Sys.FlashSize << 10) - (64 << 10);
	const uint maxAddr = 0x8000000 + (Sys.FlashSize << 10);
	if (addr<minAddr || addr>maxAddr )
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
	return true;
}

Device * DevicesManagement::Find(byte id) const
{
	if (id == 0)return nullptr;
	for (int i = 0; i < Arr.Length(); i++)
		if (id == Arr[i]->Address)return Arr[i];
	return nullptr;
}

Device * DevicesManagement::Find(const Buffer & hardid) const
{
	if (hardid.Length())return nullptr;
	for (int i = 0; i < Arr.Length(); i++)
	{
		if (Arr[i] != nullptr&&hardid == Arr[i]->HardID)return Arr[i];
	}
	return nullptr;
}

bool DevicesManagement::Delete(byte id)
{
	TS("DevicesManagement::DeleteDevice");

	auto	dv = Find(id);
	if (dv&&dv->Address==id)
	{
		debug_printf("DevicesManagement::DeleteDevice 删除设备 0x%02X \r\n", id);
		
		int idx = Arr.FindIndex(dv);
		debug_printf("idx~~~~~~~~~~~:%d\r\n", idx);
		if (idx >= 0)Arr[idx] = nullptr;
		delete dv;
		Save();

		return true;
	}

	return false;
}

const Config DevicesManagement::GetStore(Flash & flash)
{
	if (_Addr == 0)
	{
		debug_printf("还未设置设备储存区地址\r\n");
		debug_printf("使用默认最后4K空间设置\r\n");
		const uint addr = 0x8000000 + (Sys.FlashSize << 10) - (4 << 10);
		SetFlashCfg(addr, 4 << 10);
	}
	Config cfg(flash, _Addr, _FlashSize);
	return cfg;
}

int DevicesManagement::Load()
{
	TS("DevicesManagement::Load");

	debug_printf("DevicesManagement::Load 加载设备！\r\n");
	Flash flash;
	auto cfg = GetStore(flash);

	byte* data = (byte*)cfg.Get("Devs");
	if (!data) return -1;

	Stream ms(data, _FlashSize);
	// 设备个数
	int count = ms.ReadByte();
	debug_printf("\t共有%d个节点\r\n", count);
	int i = 0;
	for (; i<count; i++)
	{
		debug_printf("\t加载设备:");

		bool fs = false;
		/*ms.Seek(1);
		byte id = ms.Peek();
		ms.Seek(-1);*/
		// 前面一个字节是长度，第二个字节才是ID
		byte id = ms.GetBuffer()[1];
		auto dv = Find(id);
		if (!dv)
		{
			dv = new Device();
			fs = true;
		}
		dv->Read(ms);
		dv->Show();

		if (fs)
		{
			int idx = Arr.FindIndex(nullptr);
			if (idx == -1)
			{
				if (dv->Valid())
					Arr.Push(dv);
				else
					delete dv;
				//debug_printf("\t Push");
			}
		}
		debug_printf("\r\n");
	}


	debug_printf("TinyServer::Load 从 0x%08X 加载 %d 个设备！\r\n", cfg.Address, i);

	byte len = Arr.Length();
	debug_printf("Devices内已有节点 %d 个\r\n", len);

	return i;
}

void DevicesManagement::Save()
{
	TS("DevicesManagement::Save");

	Flash flash;
	auto cfg = GetStore(flash);

	byte buf[0x800];

	MemoryStream ms(buf, ArrayLength(buf));
	byte num = 0;
	if (Arr.Length() == 0)
		num = 1;

	for (int i = 0; i<Arr.Length(); i++)
	{
		auto dv = Arr[i];
		if (dv == nullptr) continue;
		num++;
	}
	// 设备个数
	int count = num;
	ms.Write((byte)count);

	for (int i = 0; i<Arr.Length(); i++)
	{
		auto dv = Arr[i];
		if (dv == nullptr) continue;
		dv->Write(ms);
	}
	debug_printf("DevicesManagement::SaveDevices 保存 %d 个设备到 0x%08X！\r\n", num, cfg.Address);
	cfg.Set("Devs", Buffer(ms.GetBuffer(), ms.Position()));
}

void DevicesManagement::Clear()
{
	TS("DevicesManagement::ClearDevices");

	Flash flash;
	auto cfg = GetStore(flash);

	debug_printf("DevicesManagement::ClearDevices 清空设备列表 0x%08X \r\n", cfg.Address);

	for (int i = 0; i<Arr.Length(); i++)
	{
		if (Arr[i])delete Arr[i];
	}
	Arr.SetLength(0);	// 清零后需要保存一下，否则重启后 Length 可能不是 0。做到以防万一
	Save();
}

