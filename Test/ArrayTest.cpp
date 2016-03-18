#include "Sys.h"

void TestArray()
{
	TS("TestArray");
	
	debug_printf("TestArray......\r\n");
	
	//不同长度的原始数据
	byte bs1[] = {1,2,3,4,5};
	byte bs2[] = {6,7,8,9};
	byte bs3[] = {10,11,12,13,14,15,16,17,18,19,20};
		
	Array arr1(bs1,sizeof(bs1));
	arr1.Show(true);
	
	assert(arr1.GetBuffer() == (byte*)bs1&&arr1.Length()== sizeof(bs1),"Array(void* data, int len)");
	assert(arr1[0] == 1, " byte& operator[](int i)");
	
	//Buffer buf(bs2,sizeof(bs2));		
	arr1 = bs2;
	assert(arr1[0] == bs2[0]&&arr1[3] == bs2[3], "Array& operator = (const void* p);");
			
	arr1.Clear();
	assert(arr1[1] == 0, "virtual void Clear()");
	
	arr1.Set(bs1,sizeof(bs1));
	assert(arr1== bs1, "bool Set(void* data, int len)");
	
	arr1.SetItem((void*)bs3,0,sizeof(bs3));
	arr1.Show(true);
	assert(arr1[0] == 10&& arr1.Length() == sizeof(bs3) , "bool SetItem(const void* data, int index, int count);");
			
	debug_printf("Array测试完毕....../r/n");
				
}
