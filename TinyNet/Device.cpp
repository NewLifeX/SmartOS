#include "Time.h"
#include "Device.h"

/******************************** Device ********************************/

Device::Device() : HardID(0), Name(0), Pass(0)
{
	Address		= 0;
	Logined		= false;
	Kind		= 0;
	LastTime	= 0;
	Logins		= 0;
	DataSize	= 0x10;
	ConfigSize	= 0x10;

	PingTime	= 0;
	OfflineTime	= 0;
	SleepTime	= 0;

	RegTime		= 0;
	LoginTime	= 0;
}

void Device::Write(Stream& ms) const
{
	ms.Write(Address);
	ms.Write(Kind);
	ms.WriteArray(HardID);
	ms.Write(LastTime);
	ms.Write(DataSize);
	ms.Write(ConfigSize);
	

	ms.Write(PingTime);
	ms.Write(OfflineTime);
	ms.Write(SleepTime);
	ms.WriteString(Name);
}

void Device::Read(Stream& ms)
{
	Address	= ms.ReadByte();
	Kind	= ms.ReadUInt16();
	HardID	= ms.ReadArray();
	LastTime= ms.ReadUInt32();
	DataSize	= ms.ReadByte();
	ConfigSize	= ms.ReadByte();	

	PingTime	= ms.ReadUInt16();
	OfflineTime	= ms.ReadUInt16();
	SleepTime	= ms.ReadUInt16();
	Name		= ms.ReadString();
}

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
	ms.WriteString(Name);
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

#if DEBUG
String& Device::ToStr(String& str) const
{
	str = str + "Address=0x" + Address;
	str = str + " Kind=" + (byte)(Kind >> 8) + (byte)(Kind & 0xFF);
	str = str + " Name=" + Name;
	str = str + " HardID=" + HardID;

	DateTime dt;
	dt.Parse(LastTime);
	str = str + " LastTime=" + dt.ToString();

	str = str + " DataSize=" + DataSize;
	str = str + " ConfigSize=" + ConfigSize;

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
