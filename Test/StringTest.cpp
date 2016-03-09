#include "String.h"
void StringTest()
{	
	//构造函数
	String str1 = "456";
	String str2((char)'1');
	String srr3((String&)str1);  
	//String str4((String&&)str1);	
	String str5((char*)'1',1);
	
	debug_printf("10进制构造函数:.....\r\n");
	String Str6((byte)1,10);
	String Str7((short)1,10);
	String Str8((int)1,10);
	String Str9((uint)1,10);
	String Str10((Int64)1,10);
	String Str11((UInt64)1,10);
	String Str12((float)1.0);
	String Str13((double)1);
	
	Str6.Show(true);
	Str7.Show(true);
	Str8.Show(true);
	Str9.Show(true);
	Str10.Show(true);
	Str11.Show(true);
	Str12.Show(true);
	Str13.Show(true);
	
	debug_printf("16进制构造函数:.....\r\n");
	String Str14((byte)1,16);
	String Str15((short)1,16);
	String Str16((int)1,16);
	String Str17((uint)1,16);
	String Str18((Int64)1,16);
	String Str19((UInt64)1,16);
	String Str20((float)1.0);
	String Str21((double)1);
	
	Str14.Show(true);
	Str15.Show(true);
	Str16.Show(true);
	Str17.Show(true);
	Str18.Show(true);
	Str19.Show(true);
	Str20.Show(true);
	Str21.Show(true)
	
	//内存管理
	debug_printf("字符串str1");
	str1.Show(true);	
	debug_printf("字符串str1检查内存:.....\r\n");
	auto len	= str1.Length();
    auto check1 = str1.CheckCapacity(1);
	auto check2 = str1.CheckCapacity(len+1);	
	str1.GetBuffer();	
	
	//String  str2('1');
	
	str1.SetBuffer(&str2,1);
	//参数长度超标
	str1.SetBuffer(&str2,2);
	debug_printf("字符串str1 Concat 10进制.....\r\n");
	str1.Concat((String&)str2);
	str1.Show(true);
	str1.Concat( (char*)'3');
	str1.Show(true);
	str1.Concat((char) '4');
	str1.Show(true);
	str1.Concat((byte)0x10,10);
	str1.Show(true);
	str1.Concat((short)1, 10);
	str1.Show(true);
	str1.Concat((int)1, 10);
	str1.Show(true);
	str1.Concat((uint)1, 10);
	str1.Show(true);
	str1.Concat((Int64)1,10);
	str1.Show(true);
	str1.Concat((UInt64)1,10);
	str1.Show(true);
	str1.Concat((float)1.0);
	str1.Show(true);
	str1.Concat((double)1);
	str1.Show(true);
	
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

