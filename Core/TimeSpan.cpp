#include "Type.h"
#include "DateTime.h"
#include "SString.h"

#include "TimeSpan.h"

/************************************************ TimeSpan ************************************************/
TimeSpan::TimeSpan(Int64 ms)
{
	Ms	= ms;
}

TimeSpan::TimeSpan(int hours, int minutes, int seconds) : TimeSpan(0, hours, minutes, seconds) { }

TimeSpan::TimeSpan(int days, int hours, int minutes, int seconds)
{
	int val	= days * 24 * 3600 + hours * 3600 + minutes * 60 + seconds;
	Ms	= (Int64)val * 1000ULL;
}

int TimeSpan::Days()	const { return Ms / (24 * 60 * 60 * 1000); }
int TimeSpan::Hours()	const { return Ms / (60 * 60 * 1000) % 24; }
int TimeSpan::Minutes()	const { return Ms / (60 * 1000) % 60; }
int TimeSpan::Seconds()	const { return Ms / 1000 % 60; }
int TimeSpan::TotalDays()		const { return Ms / (24 * 60 * 60 * 1000); }
int TimeSpan::TotalHours()		const { return Ms / (60 * 60 * 1000); }
int TimeSpan::TotalMinutes()	const { return Ms / (60 * 1000); }
int TimeSpan::TotalSeconds()	const { return Ms / 1000; }

int TimeSpan::CompareTo(const TimeSpan& value) const
{
	return Ms - value.Ms;
}

bool operator==	(const TimeSpan& left, const TimeSpan& right) { return left.CompareTo(right) == 0; }
bool operator!=	(const TimeSpan& left, const TimeSpan& right) { return left.CompareTo(right) != 0; }
bool operator>	(const TimeSpan& left, const TimeSpan& right) { return left.CompareTo(right) >  0; }
bool operator<	(const TimeSpan& left, const TimeSpan& right) { return left.CompareTo(right) <  0; }
bool operator>=	(const TimeSpan& left, const TimeSpan& right) { return left.CompareTo(right) >= 0; }
bool operator<=	(const TimeSpan& left, const TimeSpan& right) { return left.CompareTo(right) <= 0; }

String& TimeSpan::ToStr(String& str) const
{
	str	= str + Days() + ' ';

	str	= str + Hours() + ':';

	int mnt	= Minutes();
	if(mnt < 10) str += '0';
	str	= str + mnt + ':';

	int sec	= Seconds();
	if(sec < 10) str += '0';
	str	= str + sec;

	int ms	= Ms % 1000;
	if(ms > 0)
	{
		str	+=	'.';
		if(ms < 100)
			str	+=	'0';
		else if(ms < 10)
			str	+= "00";
		str	+= ms;
	}

	return str;
}
