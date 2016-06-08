#include "Sys.h"
#include "Core\List.h"

#if DEBUG
void List::Test()
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
