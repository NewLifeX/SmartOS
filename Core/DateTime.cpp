#include "_Core.h"

#include "SString.h"
#include "DateTime.h"

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

DateTime& DateTime::Parse(uint seconds)
{
	auto& st	= *this;

	//if(seconds >= BASE_YEAR_SECONDS) seconds -= BASE_YEAR_SECONDS;

	// 分别计算毫秒、秒、分、时，剩下天数
	uint time = seconds;
    st.Second = time % 60;
    time /= 60;
    st.Minute = time % 60;
    time /= 60;
    st.Hour = time % 24;
    time /= 24;

	if(time) ParseDays(time);

	Ms	= 0;

	return st;
}

DateTime& DateTime::ParseMs(UInt64 ms)
{
	Parse(ms / 1000ULL);
	Ms = ms % 1000ULL;

	return *this;
}

DateTime& DateTime::ParseDays(uint days)
{
	// 基本年的一天不一定是星期天，需要偏移BASE_YEAR_DAYOFWEEK_SHIFT
    //DayOfWeek = (days + BASE_YEAR_DAYOFWEEK_SHIFT) % 7;
    Year = (ushort)(days / 365);
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
    Month = (ushort)(days / 31 + 1);
    int mtd = MONTH_TO_DAYS(Year, Month + 1);
    if (days >= mtd) Month++;

	// 计算月份表示的天数
    mtd = MONTH_TO_DAYS(Year, Month);

	// 今年总天数减去月份天数，得到该月第几天
    Day = (ushort)(days - mtd + 1);

	return *this;
}

DateTime::DateTime()
{
	Init();
}

DateTime::DateTime(ushort year, byte month, byte day)
{
	Year	= year;
	Month	= month;
	Day		= day;
	Hour	= 0;
	Minute	= 0;
	Second	= 0;
	Ms		= 0;
}

DateTime::DateTime(uint seconds)
{
	//if(seconds == 0)
	//	Buffer(&Year, &Ms - &Year + sizeof(Ms)).Clear();
	//else
	Parse(seconds);
}

DateTime::DateTime(const DateTime& value)
{
	*this	= value;
}

DateTime::DateTime(DateTime&& value)
{
	// 拷贝对方数据，把对方初始化为默认值
	auto bs	= ToArray();
	bs	= value.ToArray();
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

Buffer DateTime::ToArray()
{
	return Buffer(&Year, &Ms - &Year + sizeof(Ms));
}

// 重载等号运算符
DateTime& DateTime::operator=(uint seconds)
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
		auto bs	= ToArray();
		bs	= ((DateTime&)value).ToArray();
	}

	return *this;
}

uint DateTime::TotalDays() const
{
	return YEARS_TO_DAYS(Year) + MONTH_TO_DAYS(Year, Month) + Day - 1;
}

uint DateTime::TotalSeconds() const
{
	uint s = TotalDays() * 24 + Hour;
	s = s * 60 + Minute;
	s = s * 60 + Second;

	return s;
}

UInt64 DateTime::TotalMs() const
{
	UInt64 sec = (UInt64)TotalSeconds();

	return sec * 1000UL + (UInt64)Ms;
}

// 获取星期
byte DateTime::DayOfWeek() const
{
	uint days	= YEARS_TO_DAYS(Year) + MONTH_TO_DAYS(Year, Month) + Day - 1;
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
	DateTime dt;
	dt.ParseDays(TotalDays() + value);

	return dt;
}

DateTime DateTime::AddHours(int value) const
{
	/*value		+= Hour;
	int days	= value / 24;
	int hour	= value % 24;

	// 可能上下溢出，需要修正
	if(hour < 0)
	{
		days--;
		hour	+= 24;
	}

	auto dt	= AddDays(days);
	dt.Hour	= hour;

	return dt;*/

	return AddSeconds(value * 3600);
}

DateTime DateTime::AddMinutes(int value) const
{
	/*value		+= Minute;
	int hours	= value / 60;
	int mins	= value % 60;

	// 可能上下溢出，需要修正
	if(mins < 0)
	{
		hours--;
		mins	+= 60;
	}

	auto dt	= AddHours(hours);
	dt.Minute	= mins;

	return dt;*/

	return AddSeconds(value * 60);
}

DateTime DateTime::AddSeconds(int value) const
{
	DateTime dt;
	dt.Parse(TotalSeconds() + value);

	return dt;
}

DateTime DateTime::AddMilliseconds(Int64 value) const
{
	DateTime dt;
	dt.ParseMs(TotalMs() + value);

	return dt;
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

String& DateTime::ToStr(String& str) const
{
	// F长全部 yyyy-MM-dd HH:mm:ss
	/*str.Concat(Year, 10, 4);
	str	+= '-';
	str.Concat(Month, 10, 2);
	str	+= '-';
	str.Concat(Day, 10, 2);
	str	+= ' ';
	str.Concat(Hour, 10, 2);
	str	+= ':';
	str.Concat(Minute, 10, 2);
	str	+= ':';
	str.Concat(Second, 10, 2);*/
	//str = str + Year + '-' + Month + '-' + Day + ' ' + Hour + ':' + Minute + ':' + Second;
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

	/*char cs[20];
	sprintf(cs, "%04d-%02d-%02d %02d:%02d:%02d", Year, Month, Day, Hour, Minute, Second);
	str	+= cs;*/

	return str;
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
cstring DateTime::GetString(byte kind, char* str)
{
	//assert_param(str);
	//if(!str) str = _Str;

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
}
