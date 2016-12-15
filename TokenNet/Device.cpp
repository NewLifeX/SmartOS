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
	OfflineTime = 0;
	SleepTime	= 0;
	Flag.Data	= 0;

	HardID.Clear();
	Mac.Clear();
	Name.Clear();
	Pass.Clear();
	Store.Clear();

	Cfg			= nullptr;

	LastRead	= 0;
	//LastWrite	= 0;
}

Device::~Device()
{
	
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
	uint len = ms.ReadEncodeInt();
	uint p = ms.Position();
	ms.Read(bs);
	ms.SetPosition(p + len);
}

/*
void Device::WriteMessage(Stream& ms) const
{
	byte* buf = ms.Current();
	// 先写入0占位，后面回来写入大小
	ms.Write((byte)0);
	uint p = ms.Position();

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
	byte size = ms.Position() - p;
	buf[0] = size;
}

void Device::ReadMessage(Stream& ms)
{
	byte size = ms.ReadByte();
	uint p = ms.Position();

	Address = ms.ReadByte();
	Kind = ms.ReadUInt16();
	HardID = ms.ReadArray();
	LastTime = ms.ReadUInt32();

	Version = ms.ReadUInt16();
	DataSize = ms.ReadByte();
	ConfigSize = ms.ReadByte();

	SleepTime = ms.ReadUInt16();
	OfflineTime = ms.ReadUInt16();
	PingTime = ms.ReadUInt16();

	Name = ms.ReadString();

	// 最后位置
	ms.SetPosition(p + size);
}
*/

bool Device::Valid() const
{
	if (Address == 0x00)	return false;
	if (Kind == 0x0000)	return false;
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
	byte len = Store[0];
	if (len > 1 && len <= 32)
	{
		str = str + " ";
		ByteArray(Store.GetBuffer(), len).ToStr(str);
	}

	if (Name.Length() > 0)
	{
		str += "\t";
		str += Name;
	}
	if (Flag.BitFlag.OnlineAlws)
	{
		str += "  持久在线设备";
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

