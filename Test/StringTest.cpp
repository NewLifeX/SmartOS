#include "String.h"
#include "Time.h"

/*static String TestMove(String& ss)
{
	//String ss = "Hello Move";
	ss += " Test ";
	//ss += dd;

	return ss;
}*/

static void TestCtor()
{
	TS("TestCtor");

	debug_printf("字符串构造函数测试\r\n");

	String str1("456");
	assert_param2(str1 == "456", "String(const char* cstr)");
	assert_param2(str1.GetBuffer() != "456", "String(const char* cstr)");

	String str2(str1);
	assert_param2(str2 == str1, "String(const String& str)");
	assert_param2(str2.GetBuffer() != str1.GetBuffer(), "String(const String& str)");

	StringHelper str3(str1);
	assert_param2(str3 == str1, "String(StringHelper&& rval)");
	assert_param2(str3.GetBuffer() != str1.GetBuffer(), "String(StringHelper&& rval)");

	char cs[] = "Hello Buffer";
	String str4(cs, sizeof(cs));
	assert_param2(str4 == cs, "String(char* str, int length)");
	assert_param2(str4.GetBuffer() == cs, "String(char* str, int length)");

	/*debug_printf("move测试\r\n");
	auto tt	= TestMove(str1);
	tt.Show(true);
	str1.Show(true);*/

	String str5((char)'1');
	assert_param2(str5 == "1", "String(char c)");
}

void TestNum10()
{
	TS("TestNum10");

	debug_printf("10进制构造函数:.....\r\n");
	String str1((byte)123, 10);
	assert_param2(str1 == "123", "String(byte value, int radix = 10)");

	String str2((short)4567, 10);
	assert_param2(str2 == "4567", "String(short value, int radix = 10)");

	String str3((int)-88996677, 10);
	assert_param2(str3 == "-88996677", "String(int value, int radix = 10)");

	String str4((uint)0xFFFFFFFF, 10);
	assert_param2(str4 == "4294967295", "String(uint value, int radix = 10)");

	String str5((Int64)-7744, 10);
	assert_param2(str5 == "-7744", "String(Int64 value, int radix = 10)");

	String str6((UInt64)331144, 10);
	assert_param2(str6 == "331144", "String(UInt64 value, int radix = 10)");

	// 默认2位小数，所以字符串要补零
	String str7((float)123.0);
	assert_param2(str7 == "123.00", "String(float value, int decimalPlaces = 2)");

	// 浮点数格式化的时候，如果超过要求小数位数，则会四舍五入
	String str8((double)456.784);
	assert_param2(str8 == "456.78", "String(double value, int decimalPlaces = 2)");

	String str9((double)456.789);
	assert_param2(str9 == "456.79", "String(double value, int decimalPlaces = 2)");
}

void TestNum16()
{
	TS("TestNum16");

	debug_printf("16进制构造函数:.....\r\n");
	String str1((byte)0xA3, 16);
	assert_param2(str1 == "a3", "String(byte value, int radix = 16)");
	assert_param2(String((byte)0xA3, -16) == "A3", "String(byte value, int radix = 16)");

	String str2((short)0x4567, 16);
	assert_param2(str2 == "4567", "String(short value, int radix = 16)");

	String str3((int)-0x7799, 16);
	assert_param2(str3 == "ffff8867", "String(int value, int radix = 16)");

	String str4((uint)0xFFFFFFFF, 16);
	assert_param2(str4 == "ffffffff", "String(uint value, int radix = 16)");

	String str5((Int64)0x331144997AC45566, 16);
	assert_param2(str5 == "331144997ac45566", "String(Int64 value, int radix = 16)");

	String str6((UInt64)0x331144997AC45566, -16);
	assert_param2(str6 == "331144997AC45566", "String(UInt64 value, int radix = 16)");
}

static void TestAssign()
{
	TS("TestAssign");

	debug_printf("赋值构造测试\r\n");

	String str = "万家灯火，无声物联！";
	debug_printf("TestAssign: %s\r\n", str.GetBuffer());
	str	= "无声物联";
	assert_param2(str == "无声物联", "String& operator = (const char* cstr)");

	String str2	= "xxx";
	str2	= str;
	assert_param2(str == "无声物联", "String& operator = (const char* cstr)");
	assert_param2(str2.GetBuffer() != str.GetBuffer(), "String& operator = (const String& rhs)");
}

static void TestConcat()
{
	TS("TestConcat");

	debug_printf("字符串连接测试\r\n");

	auto now	= Time.Now();
	//char cs[32];
	//debug_printf("now: %d %s\r\n", now.Second, now.GetString('F', cs));

	String str;

	// 连接时间，继承自Object
	str	+= now;
	//str.Show(true);
	// yyyy-MM-dd HH:mm:ss
	assert_param2(str.Length() == 19, "String& operator += (const Object& rhs)");

	// 连接其它字符串
	int len	= str.Length();
	String str2(" 中国时间");
	str += str2;
	assert_param2(str.Length() == len + str2.Length(), "String& operator += (const String& rhs)");

	// 连接C格式字符串
	str	+= " ";

	// 连接整数
	len	= str.Length();
	str += 1234;
	assert_param2(str.Length() == len + 4, "String& operator += (int num)");

	// 连接C格式字符串
	str	+= " ";

	// 连接浮点数
	len	= str.Length();
	str += -1234.8856;	// 特别注意，默认2位小数，所以字符串是-1234.89
	str.Show(true);
	assert_param2(str.Length() == len + 8, "String& operator += (double num)");
}

static void TestConcat16()
{
	TS("TestConcat16");

	String str = "十六进制连接测试 ";

	// 连接单个字节的十六进制
	str.Concat((byte)0x20, 16);

	// 连接整数的十六进制，前面补零
	str	+= " @ ";
	str.Concat((ushort)0xE3F, 16);

	// 连接整数的十六进制（大写字母），前面补零
	str += " # ";
	str.Concat(0x73F88, -16);

	str.Show(true);
	// 十六进制连接测试 20 @ 00000e3f # 00073F88
	assert_param2(str == "十六进制连接测试 20 @ 00000e3f # 00073F88", "bool Concat(int num, int radix = 16)");
}

static void TestAdd()
{
	TS("TestAdd");

	String str = R("字符串连加 ") + 1234 + "@" + Time.Now() + "#" + R("99xx") + '$' + -33.883;
	str.Show(true);
	// 字符串连加 1234@0000-00-00 00:00:00#99xx
	assert_param2(str == "字符串连加 1234@0000-00-00 00:00:00#99xx$-33.88", "friend StringHelper& operator + (const StringHelper& lhs, const char* cstr)");
}

static void TestEquals()
{
	TS("TestEquals");

	debug_printf("字符串相等测试\r\n");

	String str1 = "TestABC HH";
	String str2 = "TESTABC HH";
	assert_param2(str1 >= str2, "bool operator <  (const String& rhs) const");
	assert_param2(str1.EqualsIgnoreCase(str2), "bool EqualsIgnoreCase(const String& s)");
}

void TestString()
{
	TS("TestString");

	TestCtor();
	TestNum10();
	TestNum16();
	TestAssign();
	TestConcat();
	TestConcat16();
	TestAdd();
	TestEquals();

	//内存管理
	debug_printf("字符串str1");
	String str1("456");
	str1.Show(true);
	debug_printf("字符串str1检查内存:.....\r\n");
	auto len	= str1.Length();
    auto check1 = str1.CheckCapacity(1);
	auto check2 = str1.CheckCapacity(len+1);
	str1.GetBuffer();

	String  str2('1');

	str1.SetBuffer(&str2,1);
	//参数长度超标
	str1.SetBuffer(&str2,2);
	debug_printf("字符串str1 Concat 10进制.....\r\n");

	debug_printf("字符串str1 Concat 16进制.....\r\n");
	str1.Concat((byte)0x1,16);
	str1.Show(true);
	str1.Concat((short)1, 16);
	str1.Show(true);
	str1.Concat((int)1, 16);
	str1.Show(true);
	str1.Concat((uint)1, 16);
	str1.Show(true);
	str1.Concat((Int64)1,16);
	str1.Show(true);
	str1.Concat((UInt64)1,16);
	str1.Show(true);
	str1.Concat((float)1.0);
	str1.Show(true);
	str1.Concat((double)1);
	str1.Show(true);

	debug_printf("字符串str1 +=操作.....\r\n");

	str1+=(Object&)str2;
	str1.Show(true);
	str1+=(String&)str2;
	str1.Show(true);
	str1+=(char*)'1';
	str1.Show(true);
	str1+=(byte)1;
	str1.Show(true);
	str1+=(int)1;
	str1.Show(true);
	str1+=(uint)1;
	str1.Show(true);
	str1+=(Int64)1;
	str1.Show(true);
	str1+=(float)1.0;
	str1.Show(true);
	str1+=(double)1;
	str1.Show(true);


}

