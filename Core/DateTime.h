#ifndef __DateTime_H__
#define __DateTime_H__

#include "Buffer.h"

// 时间日期
class DateTime : public Object
{
public:
	ushort	Year;
	byte	Month;
	byte	Day;
	byte	Hour;
	byte	Minute;
	byte	Second;
	ushort	Ms;

	DateTime();
	DateTime(ushort year, byte month, byte day);
	DateTime(uint seconds);
	DateTime(const DateTime& value);
	DateTime(DateTime&& value);

	// 重载等号运算符
    DateTime& operator=(uint seconds);
    DateTime& operator=(const DateTime& value);

	DateTime& Parse(uint seconds);
	DateTime& ParseMs(UInt64 ms);
	DateTime& ParseDays(uint days);
	
	uint TotalDays() const;
	uint TotalSeconds() const;
	UInt64 TotalMs() const;

	// 获取星期
	byte DayOfWeek() const;
	// 取时间日期的日期部分
	DateTime Date() const;

	// Add函数返回新的实例
	DateTime AddYears(int value) const;
	DateTime AddMonths(int value) const;
	DateTime AddDays(int value) const;
	DateTime AddHours(int value) const;
	DateTime AddMinutes(int value) const;
	DateTime AddSeconds(int value) const;
	DateTime AddMilliseconds(Int64 value) const;

	// 时间比较
	int CompareTo(const DateTime& value) const;
    friend bool operator==	(const DateTime& left, const DateTime& right);
    friend bool operator!=	(const DateTime& left, const DateTime& right);
    friend bool operator>	(const DateTime& left, const DateTime& right);
    friend bool operator<	(const DateTime& left, const DateTime& right);
    friend bool operator>=	(const DateTime& left, const DateTime& right);
    friend bool operator<=	(const DateTime& left, const DateTime& right);

	virtual String& ToStr(String& str) const;

	// 默认格式化时间为yyyy-MM-dd HH:mm:ss
	/*
	d短日期 M/d/yy
	D长日期 yyyy-MM-dd
	t短时间 mm:ss
	T长时间 HH:mm:ss
	f短全部 M/d/yy HH:mm
	F长全部 yyyy-MM-dd HH:mm:ss
	*/
	cstring GetString(byte kind = 'F', char* str = nullptr);

	// 当前时间
	static DateTime Now();

#if DEBUG
	static void Test();
#endif

private:
	void Init();
	Buffer ToArray();
};

#endif
