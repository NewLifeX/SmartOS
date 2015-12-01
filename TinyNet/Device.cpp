#include "Time.h"
#include "Device.h"

/******************************** Device ********************************/

Device::Device()
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
}

void Device::Write(Stream& ms) const
{
	TS("Device::Write");

	/*ms.Write(Address);
	ms.Write(Kind);
	ms.WriteArray(CArray(HardID));
	ms.Write(LastTime);
	ms.Write(RegTime);
	ms.Write(LoginTime);
	ms.Write(Logins);

	ms.Write(Version);
	ms.Write(DataSize);
	ms.Write(ConfigSize);

	ms.Write(SleepTime);
	ms.Write(OfflineTime);
	ms.Write(PingTime);

	ms.WriteArray(CArray(Name));
	ms.WriteArray(CArray(Pass));
	ms.WriteArray(CArray(Store));*/

	Array bs(&Address, offsetof(Device, Cfg) - offsetof(Device, Address));
	ms.WriteArray(bs);
}

void Device::Read(Stream& ms)
{
	TS("Device::Read");

	/*Address	= ms.ReadByte();
	Kind	= ms.ReadUInt16();
	HardID	= ms.ReadArray();
	LastTime= ms.ReadUInt32();
	RegTime	= ms.ReadUInt32();
	LoginTime= ms.ReadUInt32();
	Logins	= ms.ReadUInt32();

	Version		= ms.ReadUInt16();
	DataSize	= ms.ReadByte();
	ConfigSize	= ms.ReadByte();

	SleepTime	= ms.ReadUInt16();
	OfflineTime	= ms.ReadUInt16();
	PingTime	= ms.ReadUInt16();

	Name		= ms.ReadString();
	Pass		= ms.ReadArray();
	Store		= ms.ReadArray();*/

	// 为了避免不同版本的配置兼容，指定长度避免覆盖过头
	Array bs(&Address, offsetof(Device, Cfg) - offsetof(Device, Address));
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
	ms.WriteArray(CArray(HardID));
	ms.Write(LastTime);

	ms.Write(Version);
	ms.Write(DataSize);
	ms.Write(ConfigSize);

	ms.Write(SleepTime);
	ms.Write(OfflineTime);
	ms.Write(PingTime);

	ms.WriteArray(CArray(Name));

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
	ms.ReadArray().CopyTo(HardID, ArrayLength(HardID));
	LastTime= ms.ReadUInt32();

	Version		= ms.ReadUInt16();
	DataSize	= ms.ReadByte();
	ConfigSize	= ms.ReadByte();

	SleepTime	= ms.ReadUInt16();
	OfflineTime	= ms.ReadUInt16();
	PingTime	= ms.ReadUInt16();

	ms.ReadString().CopyTo(Name, ArrayLength(Name));

	// 最后位置
	ms.SetPosition(p + size);
}
/*
void Device::Save(Stream& ms) const
{
	ms.Write(Address);
	ms.Write(Kind);
	ms.WriteArray(HardID);
	ms.Write(LastTime);
	ms.Write(Logins);
	ms.Write(Version);
	ms.Write(DataSize);
	ms.Write(ConfigSize);
	ms.WriteArray(Name);
	ms.WriteArray(Pass);

	ms.Write(PingTime);
	ms.Write(OfflineTime);
	ms.Write(SleepTime);
	ms.Write(RegTime);
	ms.Write(LoginTime);
}

void Device::Load(Stream& ms)
{
	Address	= ms.ReadByte();
	Kind	= ms.ReadUInt16();
	HardID	= ms.ReadArray();
	LastTime= ms.ReadUInt32();
	Logins	= ms.ReadUInt32();
	Version	= ms.ReadUInt16();
	DataSize	= ms.ReadByte();
	ConfigSize	= ms.ReadByte();
	Name	= ms.ReadString();
	Pass	= ms.ReadArray();

	PingTime	= ms.ReadUInt16();
	OfflineTime	= ms.ReadUInt16();
	SleepTime	= ms.ReadUInt16();
	RegTime		= ms.ReadUInt32();
	LoginTime	= ms.ReadUInt32();
}
*/
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
	str = str + "Addr=0x" + Address;
	str = str + " Kind=" + (byte)(Kind >> 8) + (byte)(Kind & 0xFF);
	//str = str + " ID=" + HardID;
	str = str + " ID=";
	str.Append(HardID[0]).Append(HardID[1]);

	DateTime dt;
	dt.Parse(LastTime);
	str = str + " LastTime=" + dt.ToString();
/*
	str = str + "Address=0x" + Address;
	str = str + " Kind=" + (byte)(Kind >> 8) + (byte)(Kind & 0xFF);
	str = str + " Name=" + Name;
	str = str + " HardID=" + HardID;

	DateTime dt;
	dt.Parse(LastTime);
	str = str + " LastTime=" + dt.ToString();

	str = str + " DataSize=" + DataSize;
	str = str + " ConfigSize=" + ConfigSize;
*/
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

Array Device::GetHardID()	const	{ return Array(HardID, ArrayLength(HardID)); }
Array Device::GetName()		const	{ return Array(Name, ArrayLength(Name)); }
Array Device::GetPass()		const	{ return Array(Pass, ArrayLength(Pass)); }
Array Device::GetStore()	const	{ return Array(Store, ArrayLength(Store)); }
Array Device::GetConfig()	const	{ return Array(Cfg, sizeof(Cfg[0])); }
