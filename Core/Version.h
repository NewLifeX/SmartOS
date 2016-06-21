#ifndef __Version_H__
#define __Version_H__

// 版本
class Version : public Object
{
public:
	byte	Major;	// 主版本
	byte	Minor;	// 次版本
	ushort	Build;	// 编译时间。2016-01-01以来的小时数

	Version();
	Version(int value);
	Version(byte major, byte minor, ushort build);
	Version(const Version& ver);
	Version(Version&& ver);

    Version& operator=(const Version& ver);

	int ToValue() const;

	int CompareTo(const Version& value) const;
    friend bool operator==	(const Version& left, const Version& right);
    friend bool operator!=	(const Version& left, const Version& right);
    friend bool operator>	(const Version& left, const Version& right);
    friend bool operator<	(const Version& left, const Version& right);
    friend bool operator>=	(const Version& left, const Version& right);
    friend bool operator<=	(const Version& left, const Version& right);

	// 根据版本号反推编译时间。
	DateTime Compile() const;

	virtual String& ToStr(String& str) const;

	// 基本年默认2016年
	static int BaseYear;
};

#endif
