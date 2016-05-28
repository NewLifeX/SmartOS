#ifndef __Version_H__
#define __Version_H__

// 版本
class Version : public Object
{
public:
	ushort Major;
	ushort Minor;
	ushort Build;
	ushort Revision;

	Version();
	Version(UInt64 value);
	Version(ushort major, ushort minor, ushort build, ushort revision);
	Version(const Version& ver);
	Version(Version&& ver);

    Version& operator=(const Version& ver);

	UInt64 ToValue() const;

	int CompareTo(const Version& value) const;
    friend bool operator==	(const Version& left, const Version& right);
    friend bool operator!=	(const Version& left, const Version& right);
    friend bool operator>	(const Version& left, const Version& right);
    friend bool operator<	(const Version& left, const Version& right);
    friend bool operator>=	(const Version& left, const Version& right);
    friend bool operator<=	(const Version& left, const Version& right);
	
	// 根据版本号反推编译时间
	DateTime Compile() const;

	virtual String& ToStr(String& str) const;
};

#endif
