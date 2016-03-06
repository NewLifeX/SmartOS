#include "Time.h"
#include "Device.h"

/******************************** Device ********************************/

Device::Device() :
	HardID(_HardID, ArrayLength(HardID)),
	Mac(_Mac, ArrayLength(_Mac)),
	Name(_Name, ArrayLength(_Name)),
	Pass(_Pass, ArrayLength(_Pass)),
	Store(_Store, ArrayLength(_Store))
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

	/*ArrayZero(Mac);
	ArrayZero(Name);
	ArrayZero(Pass);
	ArrayZero(Store);*/
	HardID.Clear();
	Mac.Clear();
	Name.Clear();
	Pass.Clear();
	Store.Clear();

	Cfg			= NULL;

	LastRead	= 0;
	//LastWrite	= 0;
}

void Device::Write(Stream& ms) const
{
	TS("Device::Write");

	Array bs(&Address, offsetof(Device, Cfg) - offsetof(Device, Address));
	ms.WriteArray(bs);
}

void Device::Read(Stream& ms)
{
	TS("Device::Read");

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
	ms.WriteArray(HardID);
	ms.Write(LastTime);

	ms.Write(Version);
	ms.Write(DataSize);
	ms.Write(ConfigSize);

	ms.Write(SleepTime);
	ms.Write(OfflineTime);
	ms.Write(PingTime);

	ms.WriteString(Name);

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
	//ms.ReadArray().CopyTo(0, HardID, ArrayLength(HardID));
	HardID	= ms.ReadArray();
	LastTime= ms.ReadUInt32();

	Version		= ms.ReadUInt16();
	DataSize	= ms.ReadByte();
	ConfigSize	= ms.ReadByte();

	SleepTime	= ms.ReadUInt16();
	OfflineTime	= ms.ReadUInt16();
	PingTime	= ms.ReadUInt16();

	//ms.ReadString().CopyTo(Name, ArrayLength(Name));
	//String str(Name, ArrayLength(Name));
	//str	= ms.ReadString();
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
	str += "Addr=0x" + Address;
	str += " Kind=" + (byte)(Kind >> 8) + (byte)(Kind & 0xFF);
	//str = str + " ID=" + HardID;
	str += " Hard=";
	str.Concat(HardID[0], 16);
	str.Concat(HardID[1], 16);
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

/*Array Device::GetHardID()	{ return Array(HardID, ArrayLength(HardID)); }
Array Device::GetName()		{ return Array(Name, ArrayLength(Name)); }
Array Device::GetPass()		{ return Array(Pass, ArrayLength(Pass)); }
Array Device::GetStore()	{ return Array(Store, ArrayLength(Store)); }
Array Device::GetConfig()	{ return Array(Cfg, sizeof(Cfg[0])); }

const Array Device::GetHardID()	const	{ return Array(HardID, ArrayLength(HardID)); }
const Array Device::GetName()	const	{ return Array(Name, ArrayLength(Name)); }
const Array Device::GetPass()	const	{ return Array(Pass, ArrayLength(Pass)); }
const Array Device::GetStore()	const	{ return Array(Store, ArrayLength(Store)); }
const Array Device::GetConfig()	const	{ return Array(Cfg, sizeof(Cfg[0])); }

void Device::SetHardID(const Array& arr)	{ arr.CopyTo(HardID, ArrayLength(HardID)); }
void Device::SetName(const Array& arr)		{ arr.CopyTo(Name, ArrayLength(Name)); }
void Device::SetPass(const Array& arr)		{ arr.CopyTo(Pass, ArrayLength(Pass)); }
void Device::SetStore(const Array& arr)		{ arr.CopyTo(Store, ArrayLength(Store)); }
void Device::SetConfig(const Array& arr)	{ arr.CopyTo(Cfg, sizeof(Cfg[0])); }*/
