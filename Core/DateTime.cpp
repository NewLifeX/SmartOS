#include "_Core.h"

#include "SString.h"
#include "DateTime.h"

#if defined(__CC_ARM)
	#include <time.h>
#else
	#include <ctime>
#endif

/************************************************ DateTime ************************************************/

// 我们的时间起点是 1/1/1970 00:00:00.000 在公历里面1/1/1970是星期四
#define BASE_YEAR                   1970
// 截止基本年，已过去的闰年数。1969 / 4 - 1969 / 100 + 1969 / 400 = 492 - 19 + 4 = 477
#define BASE_YEAR_LEAPYEAR_ADJUST   (((BASE_YEAR - 1) / 4) - ((BASE_YEAR - 1) / 100) + ((BASE_YEAR - 1) / 400))
#define BASE_YEAR_DAYOFWEEK_SHIFT   4		// 星期偏移

// 基本年的秒数
#define BASE_YEAR_SECONDS			62135596800UL

const int CummulativeDaysForMonth[13] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};

#define IS_LEAP_YEAR(y)             (((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0))
// 基于基本年的闰年数，不包含当年
#define NUMBER_OF_LEAP_YEARS(y)     ((((y - 1) / 4) - ((y - 1) / 100) + ((y - 1) / 400)) - BASE_YEAR_LEAPYEAR_ADJUST)
#define NUMBER_OF_YEARS(y)          (y - BASE_YEAR)

#define YEARS_TO_DAYS(y)            ((NUMBER_OF_YEARS(y) * 365) + NUMBER_OF_LEAP_YEARS(y))
#define MONTH_TO_DAYS(y, m)         (CummulativeDaysForMonth[m - 1] + ((IS_LEAP_YEAR(y) && (m > 2)) ? 1 : 0))

DateTime::DateTime()
{
	Init();
}

DateTime::DateTime(int year, int month, int day)
{
	if(year < BASE_YEAR) year	= BASE_YEAR;
	if(month < 1 || month > 12) month	= 1;
	if(day < 1 || day > 31) day	= 1;

	Year	= year;
	Month	= month;
	Day		= day;
	Hour	= 0;
	Minute	= 0;
	Second	= 0;
	Ms		= 0;
}

DateTime::DateTime(int seconds)
{
	Parse(seconds);

	Ms	= 0;
}

DateTime::DateTime(const DateTime& value)
{
	*this	= value;
}

DateTime::DateTime(DateTime&& value)
{
	// 拷贝对方数据，把对方初始化为默认值
	ToArray()	= value.ToArray();
	// 对方反正不用了，就不要浪费时间做初始化啦，反正没有需要释放的资源
	//value.Init();
}

void DateTime::Init()
{
	//Buffer(&Year, &Ms - &Year + sizeof(Ms)).Clear();
	Year	= BASE_YEAR;
	Month	= 1;
	Day		= 1;
	Hour	= 0;
	Minute	= 0;
	Second	= 0;
	Ms		= 0;
}

DateTime& DateTime::Parse(int seconds)
{
	auto& st	= *this;

	//if(seconds >= BASE_YEAR_SECONDS) seconds -= BASE_YEAR_SECONDS;

	// 分别计算毫秒、秒、分、时，剩下天数
	int time = seconds;
    st.Second = time % 60;
    time /= 60;
    st.Minute = time % 60;
    time /= 60;
    st.Hour = time % 24;
    time /= 24;

	ParseDays(time);

	return st;
}

DateTime& DateTime::ParseMs(Int64 ms)
{
	Parse((int)(ms / 1000LL));
	Ms = ms % 1000LL;

	return *this;
}

DateTime& DateTime::ParseDays(int days)
{
	// 基本年的一天不一定是星期天，需要偏移BASE_YEAR_DAYOFWEEK_SHIFT
    //DayOfWeek = (days + BASE_YEAR_DAYOFWEEK_SHIFT) % 7;
    Year = (short)(days / 365);
	if(Year < 136) Year += BASE_YEAR;

	// 按最小每年365天估算，如果不满当年总天数，年份减一
    int ytd = YEARS_TO_DAYS(Year);
    if (ytd > days)
    {
        Year--;
        ytd = YEARS_TO_DAYS(Year);
    }

	// 减去年份的天数
    days -= ytd;

	// 按最大每月31天估算，如果超过当月总天数，月份加一
    Month = (byte)(days / 31 + 1);
    int mtd = MONTH_TO_DAYS(Year, Month + 1);
    if (days >= mtd) Month++;

	// 计算月份表示的天数
    mtd = MONTH_TO_DAYS(Year, Month);

	// 今年总天数减去月份天数，得到该月第几天
    Day = (byte)(days - mtd + 1);

	return *this;
}

Buffer DateTime::ToArray()
{
	return Buffer(&Year, &Ms - &Year + sizeof(Ms));
}

const Buffer DateTime::ToArray() const
{
	return Buffer((void*)&Year, (int)&Ms - (int)&Year + sizeof(Ms));
}

// 重载等号运算符
DateTime& DateTime::operator=(int seconds)
{
	Parse(seconds);

	return *this;
}

DateTime& DateTime::operator=(const DateTime& value)
{
	// 避免自己拷贝自己
	if(this != &value)
	{
		// 拷贝对方数据
		ToArray()	= value.ToArray();
	}

	return *this;
}

int DateTime::TotalDays() const
{
	return YEARS_TO_DAYS(Year) + MONTH_TO_DAYS(Year, Month) + Day - 1;
}

int DateTime::TotalSeconds() const
{
	int s = TotalDays() * 24 + Hour;
	s = s * 60 + Minute;
	s = s * 60 + Second;

	return s;
}

Int64 DateTime::TotalMs() const
{
	Int64 sec = (Int64)TotalSeconds();

	return sec * 1000LL + (Int64)Ms;
}

// 获取星期
byte DateTime::DayOfWeek() const
{
	int days	= YEARS_TO_DAYS(Year) + MONTH_TO_DAYS(Year, Month) + Day - 1;
	return (days + BASE_YEAR_DAYOFWEEK_SHIFT) % 7;
}

// 取时间日期的日期部分
DateTime DateTime::Date() const
{
	return DateTime(Year, Month, Day);
}

DateTime DateTime::AddYears(int value) const
{
	auto dt	= *this;
	dt.Year	+= value;

	return dt;
}

DateTime DateTime::AddMonths(int value) const
{
	value	+= Month;
	int ys	= value / 12;
	int ms	= value % 12;

	// 可能上下溢出，需要修正
	if(ms <= 0)
	{
		ys--;
		ms	+= 12;
	}

	auto dt	= *this;
	dt.Year		+= ys;
	dt.Month	= ms;

	return dt;
}

DateTime DateTime::AddDays(int value) const
{
	auto dt	= *this;
	dt.ParseDays(TotalDays() + value);

	return dt;
}

DateTime DateTime::AddHours(int value) const
{
	return AddSeconds(value * 3600);
}

DateTime DateTime::AddMinutes(int value) const
{
	return AddSeconds(value * 60);
}

DateTime DateTime::AddSeconds(int value) const
{
	auto dt	= *this;
	dt.Parse(TotalSeconds() + value);

	return dt;
}

DateTime DateTime::AddMilliseconds(Int64 value) const
{
	DateTime dt;
	dt.ParseMs(TotalMs() + value);

	return dt;
}

DateTime DateTime::Add(const TimeSpan& value) const { return AddMilliseconds(value.TotalMs()); }
DateTime DateTime::operator+(const TimeSpan& value) { return AddMilliseconds(value.TotalMs()); }
DateTime DateTime::operator-(const TimeSpan& value) { return AddMilliseconds(-value.TotalMs()); }

TimeSpan operator-(const DateTime& left, const DateTime& right)
{
	Int64 ms	= left.TotalMs() - right.TotalMs();

	return TimeSpan(ms);
}

int DateTime::CompareTo(const DateTime& value) const
{
	int n	= (int)Year	- value.Year;
	if(n) return n;

	n		= (int)Month	- value.Month;
	if(n) return n;

	n		= (int)Day	- value.Day;
	if(n) return n;

	n		= (int)Hour	- value.Hour;
	if(n) return n;

	n		= (int)Minute	- value.Minute;
	if(n) return n;

	n		= (int)Second	- value.Second;
	if(n) return n;

	n		= (int)Ms	- value.Ms;
	if(n) return n;

	return 0;
}

bool operator==	(const DateTime& left, const DateTime& right) { return left.CompareTo(right) == 0; }
bool operator!=	(const DateTime& left, const DateTime& right) { return left.CompareTo(right) != 0; }
bool operator>	(const DateTime& left, const DateTime& right) { return left.CompareTo(right) >  0; }
bool operator<	(const DateTime& left, const DateTime& right) { return left.CompareTo(right) <  0; }
bool operator>=	(const DateTime& left, const DateTime& right) { return left.CompareTo(right) >= 0; }
bool operator<=	(const DateTime& left, const DateTime& right) { return left.CompareTo(right) <= 0; }

String DateTime::ToString() const
{
	// F长全部 yyyy-MM-dd HH:mm:ss
	String str;
	if(Year < 10) str += '0';
	if(Year < 100) str += '0';
	if(Year < 1000) str += '0';
	str = str + Year + '-';

	if(Month < 10) str += '0';
	str	= str + Month + '-';

	if(Day < 10) str += '0';
	str	= str + Day + ' ';

	if(Hour < 10) str += '0';
	str	= str + Hour + ':';

	if(Minute < 10) str += '0';
	str	= str + Minute + ':';

	if(Second < 10) str += '0';
	str	= str + Second;

	return str;
}

void DateTime::Show(bool newLine) const
{
	ToString().Show(newLine);
}

// 默认格式化时间为yyyy-MM-dd HH:mm:ss
/*
	d短日期 M/d/yy
	D长日期 yyyy-MM-dd
	t短时间 mm:ss
	T长时间 HH:mm:ss
	f短全部 M/d/yy HH:mm
	F长全部 yyyy-MM-dd HH:mm:ss
*/
/*cstring DateTime::GetString(byte kind, char* str)
{
	auto& st = *this;
	switch(kind)
	{
		case 'd':
			sprintf(str, "%d/%d/%02d", st.Month, st.Day, st.Year % 100);
			break;
		case 'D':
			sprintf(str, "%04d-%02d-%02d", st.Year, st.Month, st.Day);
			break;
		case 't':
			sprintf(str, "%02d:%02d", st.Hour, st.Minute);
			break;
		case 'T':
			sprintf(str, "%02d:%02d:%02d", st.Hour, st.Minute, st.Second);
			break;
		case 'f':
			sprintf(str, "%d/%d/%02d %02d:%02d", st.Month, st.Day, st.Year % 100, st.Hour, st.Minute);
			break;
		case 'F':
			sprintf(str, "%04d-%02d-%02d %02d:%02d:%02d", st.Year, st.Month, st.Day, st.Hour, st.Minute, st.Second);
			break;
		default:
			//assert_param(false);
			break;
	}

	return str;
}*/

// 当前时间
DateTime DateTime::Now()
{
	DateTime dt((int)time(NULL));

	return dt;
}
