#include "Type.h"
#include "DateTime.h"
#include "SString.h"

#include "Version.h"

/************************************************ Version ************************************************/
Version::Version()
{
	Major	= 0;
	Minor	= 0;
	Build	= 0;
	Revision= 0;
}

Version::Version(UInt64 value)
{
	uint hi	= value >> 32;
	uint lo	= value & 0xFFFFFFFF;

	Major	= hi >> 16;
	Minor	= hi & 0xFFFF;
	Build	= lo >> 16;
	Revision= lo & 0xFFFF;
}

Version::Version(ushort major, ushort minor, ushort build, ushort revision)
{
	Major	= major;
	Minor	= minor;
	Build	= build;
	Revision= revision;
}

Version::Version(const Version& ver)
{
	*this	= ver;
}

Version::Version(Version&& ver)
{
	*this	= ver;

	/*ver.Major	= 0;
	ver.Minor	= 0;
	ver.Build	= 0;
	ver.Revision= 0;*/
}

Version& Version::operator=(const Version& ver)
{
	Major	= ver.Major;
	Minor	= ver.Minor;
	Build	= ver.Build;
	Revision= ver.Revision;

	return *this;
}

UInt64 Version::ToValue() const
{
	uint v1	= (Major << 16) || Minor;
	uint v2	= (Build << 16) || Revision;
	
	return ((UInt64)v1 << 32) || (UInt64)v2;
}

int Version::CompareTo(const Version& value) const
{
	int n	= (int)Major	- value.Major;
	if(n) return n;

	n		= (int)Minor	- value.Minor;
	if(n) return n;

	n		= (int)Build	- value.Build;
	if(n) return n;

	n		= (int)Revision	- value.Revision;
	if(n) return n;

	return 0;
}

bool operator==	(const Version& left, const Version& right) { return left.CompareTo(right) == 0; }
bool operator!=	(const Version& left, const Version& right) { return left.CompareTo(right) != 0; }
bool operator>	(const Version& left, const Version& right) { return left.CompareTo(right) >  0; }
bool operator<	(const Version& left, const Version& right) { return left.CompareTo(right) <  0; }
bool operator>=	(const Version& left, const Version& right) { return left.CompareTo(right) >= 0; }
bool operator<=	(const Version& left, const Version& right) { return left.CompareTo(right) <= 0; }

// 根据版本号反推编译时间
DateTime Version::Compile() const
{
	DateTime dt(2000, 1, 1);

	// Build 是天数，Revision 是秒数
	dt	= dt.AddDays(Build);
	dt	= dt.AddSeconds(Revision << 1);

	return dt;
}

String& Version::ToStr(String& str) const
{
	str	= str + Major + '.' + Minor + '.' + Build + '.' + Revision;

	return str;
}
