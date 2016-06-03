#include "Sys.h"
#include "List.h"
#include "Dictionary.h"

#if DEBUG
void TestDictionary()
{
	TS("TestDictionary");

	debug_printf("TestDictionary......\r\n");

	//不同长度的原始数据
	byte buf1[] = {1,2,3,4,5};
	byte buf2[] = {6,7,8,9};
	byte buf3[] = {10,11,12,13,14,15,16,17,18,19,20};

	Dictionary dic;
	dic.Add("buf1", buf1);
	dic.Add("buf2", buf2);
	dic.Add("buf3", buf3);

	assert(dic.Count() == 3, "Count()");
	assert(dic["buf1"] == buf1 && dic["buf2"] == buf2 && dic["buf3"] == buf3, "void* operator[](const void* key) const");

	// 赋值
	dic["buf2"]	= buf3;
	dic["buf3"]	= buf2;
	assert(dic.Count() == 3, "Count()");
	assert(dic["buf2"] == buf2 && dic["buf3"] == buf3, "void*& operator[](const void* key)");

	// 查找
	bool rs	= dic.ContainKey("buf2");
	assert(rs, "bool ContainKey(const void* key)");
	rs	= dic.ContainKey("buf");
	assert(!rs, "bool ContainKey(const void* key)");

	// 删除倒数第二个。后面前移
	dic.RemoveKey("buf2");	// 无效
	assert(dic.Count() == 2, "Count()");
	assert(dic["buf2"] == nullptr, "void RemoveKey(const void* key)");

	// 尝试获取值
	void* p	= nullptr;
	rs	= dic.TryGetValue("buf3", p);
	assert(dic.Count() == 3, "Count()");
	assert(rs, "bool TryGetValue(const void* key, void*& value) const");
	// 前面曾经赋值，所以buf3里面保存的是buf2
	assert(p == buf2, "bool TryGetValue(const void* key, void*& value) const");

	debug_printf("TestDictionary测试完毕......\r\n");
}
#endif
