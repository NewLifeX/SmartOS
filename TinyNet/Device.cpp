#include "Time.h"
#include "Device.h"

/******************************** Device ********************************/

Device::Device() : HardID(0), Name(0), Pass(0)
{
	Address		= 0;
	Type		= 0;
	LastTime	= 0;
	Switchs		= 0;
	Analogs		= 0;

	RegTime		= 0;
	LoginTime	= 0;
}

void Device::Write(Stream& ms) const
{
	ms.Write(Address);
	ms.Write(Type);
	ms.WriteArray(HardID);
	ms.Write(LastTime);
	ms.Write(Switchs);
	ms.Write(Analogs);
	ms.WriteString(Name);
}

void Device::Read(Stream& ms)
{
	Address	= ms.Read<byte>();
	Type	= ms.Read<ushort>();
	//ms.ReadArray(HardID);
	HardID	= ms.ReadArray();
	LastTime= ms.Read<ulong>();
	Switchs	= ms.Read<byte>();
	Analogs	= ms.Read<byte>();
	Name	= ms.ReadString();
}

String& Device::ToStr(String& str) const
{
	str = str + "Address=0x" + Address;
	str = str + " Type=" + (byte)(Type >> 8) + (byte)(Type & 0xFF);
	str = str + " Name=" + Name;
	str = str + " HardID=" + HardID;

	DateTime dt;
	dt.Parse(LastTime);
	str = str + " LastTime=" + dt.ToString();

	str = str + " Switchs=" + Switchs;
	str = str + " Analogs=" + Analogs;

	return str;
}

bool operator==(const Device& d1, const Device& d2)
{
	return d1.Address == d2.Address;
}

bool operator!=(const Device& d1, const Device& d2)
{
	return d1.Address != d2.Address;
}
