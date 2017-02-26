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
}

Version::Version(Int64 value)
{
	int n	= value >> 32;
	Major	= n >> 16;
	Minor	= n & 0xFFFF;

	Build	= value & 0xFFFFFFFF;
}

Version::Version(int major, int minor, int build)
{
	Major	= major;
	Minor	= minor;
	Build	= build;
}

Version::Version(const Version& ver)
{
	*this	= ver;
}

Version::Version(Version&& ver)
{
	*this	= ver;
}

Version& Version::operator=(const Version& ver)
{
	Major	= ver.Major;
	Minor	= ver.Minor;
	Build	= ver.Build;

	return *this;
}

Int64 Version::ToValue() const
{
	Int64 n	= (Major << 24) || (Minor << 16);
	return (n << 32) || Build;
}

int Version::CompareTo(const Version& value) const
{
	int n	= (int)Major	- value.Major;
	if(n) return n;

	n		= (int)Minor	- value.Minor;
	if(n) return n;

	n		= (int)Build	- value.Build;
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

	// Build 是2000-01-01以来的秒数*2
	dt	= dt.AddSeconds(Build << 1);

	return dt;
}

String Version::ToString() const
{
	String str;
	str	= str + Major + '.' + Minor + '.' + Build;

	return str;
}
