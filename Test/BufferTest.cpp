#include "Sys.h"

void TestBuffer()
{
	TS("TestBuffer");

	// 使用指针和长度构造一个内存区
	char cs[] = "This is Buffer Test.";
	Buffer bs(cs, strlen(cs));
	debug_printf("Buffer bs(cs, strlen(cs)) => %s\r\n", cs);
	assert_param2(bs.GetBuffer() == (byte*)cs, "Buffer(void* p = nullptr, int len = 0)");
	assert_param2(bs == cs, "Buffer(void* p = nullptr, int len = 0)");

	byte buf[5];
	buf[4]	= '\0';
	Buffer bs2(buf, 4);
	// 拷贝长度为两者最小者，除非当前对象能自动扩容
	bs2	= bs;
	debug_printf("bs2	= bs => %s\r\n", buf);
	assert_param2(bs2.GetBuffer() != bs.GetBuffer(), "Buffer& operator = (const Buffer& rhs)");
	assert_param2(bs2 == bs, "Buffer& operator = (const Buffer& rhs)");
	assert_param2(bs2.Length() == 4, "Buffer& operator = (const Buffer& rhs)");
	assert_param2(bs2 == cs, "Buffer& operator = (const Buffer& rhs)");

	// 从指针拷贝，使用我的长度
	bs2	= cs + 8;
	debug_printf("bs2	= cs + 8 => %s\r\n", buf);
	assert_param2(bs2.GetBuffer() != (byte*)(cs + 8), "Buffer& operator = (const void* p)");
	assert_param2(bs2.Length() == 4, "Buffer& operator = (const void* p)");
	assert_param2(bs2 == cs + 8, "Buffer& operator = (const void* p)");

	// 设置数组长度。只能缩小不能扩大，子类可以扩展以实现自动扩容
	bs.SetLength(11);
	cs[11]	= '\0';
	debug_printf("SetLength(11) => %s\r\n", cs);
	assert_param2(bs.Length() == 11, "bool SetLength(int len, bool bak = false)");
	assert_param2(bs == cs, "bool SetLength(int len, bool bak = false)");

	// 索引访问
	bs[8]++;
	debug_printf("bs[8]++ => %s\r\n", cs);
	assert_param2(cs[8] == 'C', "byte& operator[](int i)");
	assert_param2(bs == cs, "byte& operator[](int i)");

	// 拷贝数据，默认-1长度表示当前长度
	char abc[]	= "abcd";
	bs.Copy(5, abc, strlen(abc));
	debug_printf("Copy(5, \"abcd\", %d) => %s\r\n", strlen(abc), cs);
	assert_param2(cs[5] == 'a' && cs[8] == 'd', "int Copy(int destIndex, const void* src, int len)");

	// 把数据复制到目标缓冲区，默认-1长度表示当前长度
	char ef[4];
	ef[3]	= 0;
	bs.CopyTo(1, ef, 3);
	debug_printf("CopyTo(1, ef, 3) => %s\r\n", ef);
	assert_param2(ef[0] == 'h' && ef[2] == 's', "int CopyTo(int srcIndex, void* dest, int len)");

	// 用指定字节设置初始化一个区域
	bs.Set('x', 3, 2);
	debug_printf("Set('x', 3, 2) => %s\r\n", cs);
	assert_param2(cs[3] == 'x' || cs[4] == 'x', "int Set(byte item, int index, int len)");

	// 截取一个子缓冲区
	auto bs3	= bs.Sub(3, 2);
	debug_printf("bs.Sub(3, 2) => %s\r\n", bs3.GetBuffer());
	assert_param2(bs3.GetBuffer() == (byte*)(cs + 3), "Buffer Sub(int index, int len)");
	assert_param2(bs3[0] == 'x' || bs3[1] == 'x', "Buffer Sub(int index, int len)");

	bs.Clear();
	assert_param2(cs[0] == 0 && cs[bs.Length() - 1] == 0, "void Clear()");
	
	// 转为十六进制字符串
	byte bts[]	= { 0xAB, 0x34, 0xfe };
	Buffer bs4(bts, 3);
	auto str	= bs4.ToHex();
	assert_param2(str == "AB-34-FE", "String ToHex()");
	
	debug_printf("内存缓冲区单元测试全部通过！");
}
