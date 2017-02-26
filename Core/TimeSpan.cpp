#include "Type.h"
#include "DateTime.h"
#include "SString.h"

#include "TimeSpan.h"

/************************************************ TimeSpan ************************************************/
TimeSpan::TimeSpan(Int64 ms)
{
	_Seconds	= (int)(ms / 1000);
	_Ms	= ms % 1000;
}

TimeSpan::TimeSpan(int hours, int minutes, int seconds) : TimeSpan(0, hours, minutes, seconds) { }

TimeSpan::TimeSpan(int days, int hours, int minutes, int seconds)
{
	_Seconds	= ((days * 24 + hours) * 60 + minutes) * 60 + seconds;
	_Ms	= 0;
}

int TimeSpan::Days()	const { return _Seconds / (24 * 60 * 60); }
int TimeSpan::Hours()	const { return _Seconds / (60 * 60) % 24; }
int TimeSpan::Minutes()	const { return _Seconds / 60 % 60; }
int TimeSpan::Seconds()	const { return _Seconds % 60; }
int TimeSpan::Ms()		const { return _Ms; }
int TimeSpan::TotalDays()		const { return _Seconds / (24 * 60 * 60); }
int TimeSpan::TotalHours()		const { return _Seconds / (60 * 60); }
int TimeSpan::TotalMinutes()	const { return _Seconds / 60; }
int TimeSpan::TotalSeconds()	const { return _Seconds; }
Int64 TimeSpan::TotalMs()		const { return _Seconds * 1000LL + (Int64)_Ms; }

int TimeSpan::CompareTo(const TimeSpan& value) const
{
	return _Ms - value._Ms;
}

bool operator==	(const TimeSpan& left, const TimeSpan& right) { return left.CompareTo(right) == 0; }
bool operator!=	(const TimeSpan& left, const TimeSpan& right) { return left.CompareTo(right) != 0; }
bool operator>	(const TimeSpan& left, const TimeSpan& right) { return left.CompareTo(right) >  0; }
bool operator<	(const TimeSpan& left, const TimeSpan& right) { return left.CompareTo(right) <  0; }
bool operator>=	(const TimeSpan& left, const TimeSpan& right) { return left.CompareTo(right) >= 0; }
bool operator<=	(const TimeSpan& left, const TimeSpan& right) { return left.CompareTo(right) <= 0; }

String TimeSpan::ToString() const
{
	String str;

	if(_Seconds >= 24 * 60 * 60) str	= str + Days() + ' ';

	str	= str + Hours() + ':';

	int mnt	= Minutes();
	if(mnt < 10) str += '0';
	str	= str + mnt + ':';

	int sec	= Seconds();
	if(sec < 10) str += '0';
	str	= str + sec;

	int ms	= _Ms;
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

void TimeSpan::Show(bool newLine) const
{
	ToString().Show(newLine);
}
