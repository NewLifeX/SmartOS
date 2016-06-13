#include "Sys.h"
#include "List.h"
#include "Dictionary.h"

#if DEBUG
void Dictionary::Test()
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

	auto err	= "void* operator[](const void* key) const";
	assert(dic.Count() == 3, err);
	assert(dic["buf1"] == buf1 && dic["buf2"] == buf2 && dic["buf3"] == buf3, err);

	// 赋值
	dic["buf2"]	= buf3;
	dic["buf3"]	= buf2;
	err	= "void*& operator[](const void* key)";
	assert(dic.Count() == 3, err);
	assert(dic["buf2"] == buf3 && dic["buf3"] == buf2, err);

	// 同名覆盖
	err	= "void Add(const void* key, void* value)";
	dic.Add("buf2", buf2);
	assert(dic.Count() == 3, err);
	assert(dic["buf2"] == buf2 && dic["buf3"] == buf2, err);
	dic["buf2"]	= buf3;

	// 查找
	bool rs	= dic.ContainKey("buf2");
	err	= "bool ContainKey(const void* key)";
	assert(rs, err);
	rs	= dic.ContainKey("buf");
	assert(!rs, err);

	// 删除倒数第二个。后面前移
	dic.Remove("buf2");	// 无效
	err	= "void RemoveKey(const void* key)";
	assert(dic.Count() == 2, err);
	assert(dic["buf2"] == nullptr, err);

	// 尝试获取值
	void* p	= nullptr;
	rs	= dic.TryGetValue("buf3", p);
	err	= "bool TryGetValue(const void* key, void*& value) const";
	assert(dic.Count() == 2, err);
	assert(rs, err);
	// 前面曾经赋值，所以buf3里面保存的是buf2
	assert(p == buf2, err);

	// 测试比较器
	cstring str	= "123456";
	Dictionary dic2(String::Compare);
	dic2.Add("test", (void*)str);

	char cs[5];
	cs[0]	= 't';
	cs[1]	= 'e';
	cs[2]	= 's';
	cs[3]	= 't';
	cs[4]	= '\0';
	rs	= dic2.TryGetValue(cs, p);

	err	= "Dictionary(IComparer comparer = nullptr)";
	assert(rs, err);
	// 前面曾经赋值，所以buf3里面保存的是buf2
	assert(p == str, err);

	debug_printf("TestDictionary测试完毕......\r\n");
}
#endif
