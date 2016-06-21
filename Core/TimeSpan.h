#ifndef __TimeSpan_H__
#define __TimeSpan_H__

// 时间间隔
class TimeSpan : public Object
{
public:
	Int64 Ms;

	TimeSpan(Int64 ms = 0);
	TimeSpan(int hours, int minutes, int seconds);
	TimeSpan(int days, int hours, int minutes, int seconds);

	int Days() const;
	int Hours() const;
	int Minutes() const;
	int Seconds() const;
	int TotalDays() const;
	int TotalHours() const;
	int TotalMinutes() const;
	int TotalSeconds() const;

	int CompareTo(const TimeSpan& value) const;
    friend bool operator==	(const TimeSpan& left, const TimeSpan& right);
    friend bool operator!=	(const TimeSpan& left, const TimeSpan& right);
    friend bool operator>	(const TimeSpan& left, const TimeSpan& right);
    friend bool operator<	(const TimeSpan& left, const TimeSpan& right);
    friend bool operator>=	(const TimeSpan& left, const TimeSpan& right);
    friend bool operator<=	(const TimeSpan& left, const TimeSpan& right);

	virtual String& ToStr(String& str) const;
};

#endif
