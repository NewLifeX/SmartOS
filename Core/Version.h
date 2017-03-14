#ifndef __Version_H__
#define __Version_H__

// 版本
class Version
{
public:
	byte	Major;	// 主版本
	byte	Minor;	// 次版本
	short	Build;	// 编译时间。2000年以来天数

	Version();
	Version(int value);
	Version(int major, int minor, int build);
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

	String ToString() const;
};

/*
主次版本一般最大只有几十，绝大多数时候是个位数，不需要太大。
*/

#endif
