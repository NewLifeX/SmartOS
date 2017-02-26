#ifndef __DateTime_H__
#define __DateTime_H__

#include "Buffer.h"
#include "TimeSpan.h"

// 时间日期
class DateTime
{
public:
	short	Year;
	byte	Month;
	byte	Day;
	byte	Hour;
	byte	Minute;
	byte	Second;
	short	Ms;

	DateTime();
	DateTime(int year, int month, int day);
	DateTime(int seconds);
	DateTime(const DateTime& value);
	DateTime(DateTime&& value);

	// 重载等号运算符
    DateTime& operator=(int seconds);
    DateTime& operator=(const DateTime& value);

	DateTime& Parse(int seconds);
	DateTime& ParseMs(Int64 ms);
	DateTime& ParseDays(int days);

	int TotalDays() const;
	int TotalSeconds() const;
	Int64 TotalMs() const;

	// 获取星期，0~6表示星期天到星期六
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
	DateTime Add(const TimeSpan& value) const;

	DateTime operator+(const TimeSpan& value);
	DateTime operator-(const TimeSpan& value);
    friend TimeSpan operator-(const DateTime& left, const DateTime& right);

	// 时间比较
	int CompareTo(const DateTime& value) const;
    friend bool operator==	(const DateTime& left, const DateTime& right);
    friend bool operator!=	(const DateTime& left, const DateTime& right);
    friend bool operator>	(const DateTime& left, const DateTime& right);
    friend bool operator<	(const DateTime& left, const DateTime& right);
    friend bool operator>=	(const DateTime& left, const DateTime& right);
    friend bool operator<=	(const DateTime& left, const DateTime& right);

	String ToString() const;
	void Show(bool newLine = true) const;

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
	const Buffer ToArray() const;
};

#endif
