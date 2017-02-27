#include "Kernel\Sys.h"

#if DEBUG
static void TestAssign()
{
	byte buf[] = {6,7,8,9};
	byte bts[] = {10,11,12,13,14,15,16,17,18,19,20};

	// Array = void*
	auto err	= "Array& operator = (const void* p)";

	// 左边空间足够，直接使用
	Array arr(bts, sizeof(bts));
	arr	= buf;
	assert(arr.GetBuffer() == bts, err);
	assert(arr.GetBuffer() != buf, err);
	assert(arr.Length() == sizeof(bts) && arr == buf, err);
	assert(bts[0] == buf[0] && bts[3] == buf[3], err);

	// 左边空间不足，也不自动扩容，因为不知道右边数据的长度
	auto p	= &bts[sizeof(buf)];
	Array arr2(buf, sizeof(buf));
	arr2	= p;
	assert(arr2.GetBuffer() == buf, err);
	assert(arr2.GetBuffer() != p, err);
	assert(arr2.Length() == sizeof(buf) && arr2 == p, err);
	assert(buf[0] == p[0] && buf[3] == p[3], err);
}

static void TestAssign2()
{
	byte buf[] = {6,7,8,9};
	byte bts[] = {10,11,12,13,14,15,16,17,18,19,20};

	// Array = Buffer
	auto err	= "Array& operator = (const Buffer& rhs)";

	// 左边空间不足，自动扩容
	Array arr(buf, sizeof(buf));
	Buffer bs(bts, sizeof(bts));
	arr	= bs;
	assert(arr.GetBuffer() != buf, err);
	assert(arr.GetBuffer() != bts, err);
	assert(arr.Length() == sizeof(bts) && arr == bts, err);
	assert(bts[0] != buf[0] && bts[3] != buf[3], err);

	// 左边空间足够，直接使用
	Array arr2(bts, sizeof(bts));
	Buffer bs2(buf, sizeof(buf));
	arr2	= bs2;
	assert(arr2.GetBuffer() == bts, err);
	assert(arr2.GetBuffer() != buf, err);
	assert(arr2.Length() == sizeof(buf) && arr2 == buf, err);
	assert(bts[0] == buf[0] && bts[3] == buf[3], err);
}

static void TestCopy()
{
	byte buf[] = {6,7,8,9};
	byte bts[] = {10,11,12,13,14,15,16,17,18,19,20};

	// Array::Copy(0, Buffer)
	auto err	= "int Copy(int destIndex, const Buffer& src, int srcIndex, int len)";

	// 左边空间足够，直接使用
	Array arr(buf, sizeof(buf));
	Buffer bs(bts, sizeof(bts));
	arr.Copy(1, bs, 2, 2);
	assert(arr.GetBuffer() == buf, err);
	assert(arr.GetBuffer() != bts, err);
	assert(arr.Length() == sizeof(buf), err);
	assert(buf[1] == bts[2] && buf[2] == bts[3], err);

	arr.Copy(2, bs, sizeof(bts) - 2, 3);
	assert(arr.GetBuffer() == buf, err);
	assert(arr.GetBuffer() != bts, err);
	assert(arr.Length() == sizeof(buf), err);
	assert(buf[2] == bts[sizeof(bts) - 2] && buf[3] == bts[sizeof(bts) - 1], err);

	// 左边空间不足，自动扩容
	arr.Copy(0, bs, 0, -1);
	assert(arr.GetBuffer() != buf, err);
	assert(arr.GetBuffer() != bts, err);
	assert(arr.Length() == sizeof(bts) && arr == bts, err);
}

void Array::Test()
{
	TS("TestArray");

	debug_printf("TestArray......\r\n");

	//不同长度的原始数据
	byte buf1[] = {1,2,3,4,5};
	//byte buf2[] = {6,7,8,9};
	byte buf3[] = {10,11,12,13,14,15,16,17,18,19,20};

	Array arr1(buf1, sizeof(buf1));
	arr1.Show(true);

	assert(arr1.GetBuffer() == (byte*)buf1 && arr1.Length()== sizeof(buf1),"Array(void* data, int len)");
	assert(arr1[0] == 1, " byte& operator[](int i)");

	arr1.Clear();
	assert(arr1[1] == 0, "virtual void Clear()");

	arr1.Set(buf1, sizeof(buf1));
	assert(arr1== buf1, "bool Set(void* data, int len)");

	arr1.SetItem(buf3, 0, sizeof(buf3));
	arr1.Show(true);
	assert(arr1[arr1.Length() - 1] == 10 && arr1.Length() == sizeof(buf3) , "bool SetItem(const void* data, int index, int count);");

	TestAssign();
	TestAssign2();
	TestCopy();

	debug_printf("Array测试完毕......\r\n");

}
#endif
