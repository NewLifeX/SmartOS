#include "Sys.h"

void TestArray()
{
	TS("TestArray");

	debug_printf("TestArray......\r\n");

	//不同长度的原始数据
	byte buf1[] = {1,2,3,4,5};
	byte buf2[] = {6,7,8,9};
	byte buf3[] = {10,11,12,13,14,15,16,17,18,19,20};

	Array arr1(buf1, sizeof(buf1));
	arr1.Show(true);

	assert(arr1.GetBuffer() == (byte*)buf1 && arr1.Length()== sizeof(buf1),"Array(void* data, int len)");
	assert(arr1[0] == 1, " byte& operator[](int i)");

	//Buffer buf(buf2,sizeof(buf2));
	arr1 = buf2;
	assert(arr1[0] == buf2[0]&&arr1[3] == buf2[3], "Array& operator = (const void* p);");

	arr1.Clear();
	assert(arr1[1] == 0, "virtual void Clear()");

	arr1.Set(buf1,sizeof(buf1));
	assert(arr1== buf1, "bool Set(void* data, int len)");

	arr1.SetItem((void*)buf3, 0, sizeof(buf3));
	arr1.Show(true);
	assert(arr1[0] == 10&& arr1.Length() == sizeof(buf3) , "bool SetItem(const void* data, int index, int count);");

	// Array = Buffer
	Array arr2(buf2, sizeof(buf2));
	Buffer bs2(buf3, sizeof(buf3));
	arr2	= bs2;
	assert(arr2.Length() == sizeof(buf2)&&arr2[0]==bs2[0],"Array = Buffer");

	debug_printf("Array测试完毕....../r/n");

}
