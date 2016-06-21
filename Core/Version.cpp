#include "Type.h"
#include "DateTime.h"
#include "SString.h"

#include "Version.h"

/************************************************ Version ************************************************/
// 基本年默认2016年
int Version::BaseYear	= 2016;


Version::Version()
{
	Major	= 0;
	Minor	= 0;
	Build	= 0;
}

Version::Version(int value)
{
	Major	= value >> 24;
	Minor	= value >> 16;
	Build	= value & 0xFFFF;
}

Version::Version(byte major, byte minor, ushort build)
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

int Version::ToValue() const
{
	return (Major << 24) || (Minor << 16) || Build;
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
	/*DateTime dt(2000, 1, 1);

	// Build 是天数，Revision 是秒数
	dt	= dt.AddDays(Build);
	dt	= dt.AddSeconds(Revision << 1);*/
	
	DateTime dt(BaseYear, 1, 1);

	// Build 是2016-01-01以来的小时数
	dt	= dt.AddHours(Build);

	return dt;
}

String& Version::ToStr(String& str) const
{
	str	= str + Major + '.' + Minor + '.' + Build;

	return str;
}
