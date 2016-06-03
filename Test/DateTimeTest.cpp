#include "Sys.h"

#if DEBUG
static void TestCtor()
{
	TS("TestCtor");

    DateTime dt;
	assert(dt.Year == 1970 && dt.Month == 1 && dt.Day == 1, "DateTime()");
	assert(dt.DayOfWeek() == 4, "DateTime()");
	assert(dt.Hour == 0 && dt.Minute == 0 && dt.Second == 0 && dt.Ms == 0, "DateTime()");

    DateTime dt2(2016, 5, 18);
	assert(dt2.Year == 2016 && dt2.Month == 5 && dt2.Day == 18, "DateTime(ushort year, byte month, byte day)");
	assert(dt2.DayOfWeek() == 3, "DateTime(ushort year, byte month, byte day)");
	assert(dt2.Hour == 0 && dt2.Minute == 0 && dt2.Second == 0 && dt2.Ms == 0, "DateTime(ushort year, byte month, byte day)");

    DateTime dt3(1463443233);
	assert(dt3.Year == 2016 && dt3.Month == 5 && dt3.Day == 17, "DateTime(uint seconds)");
	assert(dt3.DayOfWeek() == 2, "DateTime(uint seconds)");
	assert(dt3.Hour == 0 && dt3.Minute == 0 && dt3.Second == 33 && dt3.Ms == 00, "DateTime(uint seconds)");
}

static void TestParseMs()
{
	TS("TestCtor");
	
    DateTime dt3;
	dt3	= 1463443233;
	assert(dt3.Year == 2016 && dt3.Month == 5 && dt3.Day == 17, "DateTime(uint seconds)");
	assert(dt3.DayOfWeek() == 2, "DateTime(uint seconds)");
	assert(dt3.Hour == 0 && dt3.Minute == 0 && dt3.Second == 33 && dt3.Ms == 00, "DateTime(uint seconds)");
}

static void TestTotal()
{
	TS("TestTotal");
	
    DateTime dt	= 1463443233;
	assert(dt.TotalSeconds() == 1463443233, "TotalSeconds");
	assert(dt.TotalMs() == 1463443233UL * 1000, "TotalMs");
}

void DateTime::Test()
{
    debug_printf("\r\n");
    debug_printf("TestDateTime Start......\r\n");

	TestCtor();
	TestParseMs();
	TestTotal();

    debug_printf("\r\n TestDateTime Finish!\r\n");
}
#endif
