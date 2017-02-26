#ifndef __Version_H__
#define __Version_H__

// 版本
class Version
{
public:
	short	Major;	// 主版本
	short	Minor;	// 次版本
	int		Build;	// 编译时间

	Version();
	Version(Int64 value);
	Version(int major, int minor, int build);
	Version(const Version& ver);
	Version(Version&& ver);

    Version& operator=(const Version& ver);

	Int64 ToValue() const;

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
