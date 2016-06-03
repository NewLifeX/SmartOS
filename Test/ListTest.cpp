#include "Sys.h"
#include "List.h"

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

void TestList()
{
	TS("TestList");

	debug_printf("TestList......\r\n");

	//不同长度的原始数据
	byte buf1[] = {1,2,3,4,5};
	byte buf2[] = {6,7,8,9};
	byte buf3[] = {10,11,12,13,14,15,16,17,18,19,20};

	List list;
	list.Add(buf1);
	list.Add(buf2);
	list.Add(buf3);

	assert(list.Count() == 3, "Count()");
	assert(list[0] == buf1 && list[1] == buf2 && list[2] == buf3, "void Add(void* item)");

	// 添加
	list.Add(buf2);
	list.Add(buf3);
	assert(list.Count() == 5, "Count()");
	assert(list[3] == buf2 && list[4] == buf3, "void Add(void* item)");

	// 查找
	int idx	= list.FindIndex(buf2);
	assert(idx == 1, "int FindIndex(const void* item)");

	// 删除倒数第二个。后面前移
	list.RemoveAt(list.Count());	// 无效
	list.RemoveAt(list.Count() - 2);
	assert(list.Count() == 4, "Count()");
	assert(list[3] == buf3, "void RemoveAt(uint index)");

	// 删除具体项。后面前移
	list.Remove(buf2);
	assert(list.Count() == 3, "Count()");
	assert(list[1] == buf3 && list[2] == buf3, "void Remove(const void* item)");

	// 删除具体项。找不到的情况
	list.Remove(buf2);
	assert(list.Count() == 3, "Count()");

	// 查找。找不到的情况
	idx	= list.FindIndex(buf2);
	assert(idx == -1, "int FindIndex(const void* item)");

	debug_printf("下面添加多项内容，将会引发List重新分配并拷贝内存\r\n需要注意new/delete\r\n");
	for(int i=0; i<16; i++)
	{
		list.Add(buf1);
	}
	assert(list.Count() == 3 + 16, "Count()");
	assert(list[0] == buf1 && list[1] == buf3 && list[2] == buf3, "bool CheckCapacity(int count)");

	//TestAssign();
	//TestAssign2();
	//TestCopy();

	debug_printf("TestList测试完毕......\r\n");

}
#endif
