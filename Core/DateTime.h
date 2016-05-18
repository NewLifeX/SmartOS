#ifndef __DateTime_H__
#define __DateTime_H__

// 系统时钟
class DateTime : public Object
{
public:
	ushort Year;
	byte Month;
	byte DayOfWeek;
	byte Day;
	byte Hour;
	byte Minute;
	byte Second;
	ushort Ms;
	//ushort Us;

	DateTime();
	DateTime(ushort year, byte month, byte day);
	DateTime(uint seconds);

	// 重载等号运算符
    DateTime& operator=(uint seconds);

	DateTime& Parse(uint seconds);
	DateTime& ParseMs(UInt64 ms);
	//DateTime& ParseUs(UInt64 us);
	uint TotalSeconds();
	UInt64 TotalMs();
	//UInt64 TotalUs();

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
	const char* GetString(byte kind = 'F', char* str = nullptr);

	// 当前时间
	static DateTime Now();
};

#endif
