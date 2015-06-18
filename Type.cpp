#include "Sys.h"

void Object::Init(int size)
{
	// 清空整个对象。跳过最前面的4字节虚表
	byte* p = (byte*)this;
	p += 4;
	//memset((void*)p, 0, size - 4);
}

const char* Object::ToString()
{
	const char* str = typeid(*this).name();
	while(*str >= '0' && *str <= '9') str++;

	return str;
}

String& Object::To(String& str)
{
	str.Set(ToString());

	return str;
}

void Object::Show()
{
	String str;
	To(str);
	str.Show();
}

// 字符串转为字节数组
ByteArray::ByteArray(String& str) : Array(str.Length())
{
	DeepSet((byte*)str.ToString(), str.Length());
}

// 显示十六进制数据，指定分隔字符
String& ByteArray::ToHex(String& str, char sep, int newLine)
{
	str.Clear();
	byte* buf = GetBuffer();
	for(int i=0, k=0; i < Length(); i++, buf++)
	{
		byte b = *buf >> 4;
		str[k++] = b > 9 ? ('A' + b - 10) : ('0' + b);
		b = *buf & 0x0F;
		str[k++] = b > 9 ? ('A' + b - 10) : ('0' + b);

		if(i < Length() - 1)
		{
			if(newLine > 0 && (i + 1) % newLine == 0)
			{
				str[k++] = '\r';
				str[k++] = '\n';
			}
			else if(sep != '\0')
				str[k++] = sep;
		}
	}
	return str;
}

// 输出对象的字符串表示方式
const char* String::ToString()
{
	return GetBuffer();
}

/*String& String::Append(char ch)
{
	Push(ch);

	return *this;
}

String& String::Append(const char* str)
{
	char* p = (char*)str;
	while(*p) Push(*p++);

	return *this;
}*/

// 调试输出字符串
void String::Show(bool newLine)
{
	// 截断字符串，避免超长
	//this[Length()] = '\0';

	debug_printf("%s", ToString());
	if(newLine) debug_printf("\r\n");
}

/* IP地址 */

IPAddress IPAddress::Any(0, 0, 0, 0);
IPAddress IPAddress::Broadcast(255, 255, 255, 255);

IPAddress::IPAddress(byte ip1, byte ip2, byte ip3, byte ip4)
{
	Value = (ip4 << 24) + (ip3 << 16) + (ip2 << 8) + ip1;
}

bool IPAddress::IsAny() { return Value == 0; }

bool IPAddress::IsBroadcast() { return Value == 0xFFFFFFFF; }

uint IPAddress::GetSubNet(IPAddress& mask)
{
	return Value & mask.Value;
}

// 重载索引运算符[]，让它可以像数组一样使用下标索引。
byte& IPAddress::operator[](int i)
{
	assert_param(i >= 0 && i < 4);

	return ((byte*)&Value)[i];
}

String& IPAddress::To(String& str)
{
	byte* ips = (byte*)&Value;

	char ss[16];
	memset(ss, 0, ArrayLength(ss));
	sprintf(ss, "%d.%d.%d.%d", ips[0], ips[1], ips[2], ips[3]);
	str.DeepSet(ss, 0);

	return str;
}

IPEndPoint::IPEndPoint()
{
	Port = 0;
}

IPEndPoint::IPEndPoint(IPAddress addr, ushort port)
{
	Address = addr;
	Port	= port;
}

String& IPEndPoint::To(String& str)
{
	Address.To(str);

	char ss[5];
	memset(ss, 0, ArrayLength(ss));
	sprintf(ss, ":%d", Port);
	str.DeepSet(ss, 0, str.Length());

	return str;
}

/* MAC地址 */

MacAddress MacAddress::Empty(0);
MacAddress MacAddress::Full(0xFFFFFFFFFFFFFFFFull);

MacAddress::MacAddress(ulong v)
{
	//v4 = v;
	//v2 = v >> 32;
	Value = v;
}

// 是否广播地址，全0或全1
bool MacAddress::IsBroadcast() { return !Value || Value == 0xFFFFFFFFFFFFFFFFull; }

MacAddress& MacAddress::operator=(ulong v)
{
	//v4 = v;
	//v2 = v >> 32;

	// 下面这个写法很好，一条汇编指令即可完成，但是会覆盖当前结构体后两个字节
	//*(ulong*)this = v;

	// 下面的写法需要5条汇编指令，先放入内存，再分两次读写
	/*uint* p = (uint*)&v;
	v4 = *p++;
	v2 = *(ushort*)p;*/
	
	Value = v;

	return *this;
}

// 数值
/*ulong MacAddress::Value()
{
	ulong v = v4;
	v |= ((ulong)v2) << 32;
	return v;

	// 下面这个写法简单，但是会带上后面两个字节，需要做或运算，不划算
	//return *(ulong*)this | 0x0000FFFFFFFF;
}*/

/*bool MacAddress::operator==(MacAddress& addr1, MacAddress& addr2)
{
	return addr1.v4 == addr2.v4 && addr1.v2 == addr2.v2;
}

bool MacAddress::operator!=(MacAddress& addr1, MacAddress& addr2)
{
	return addr1.v4 != addr2.v4 || addr1.v2 != addr2.v2;
}*/

String& MacAddress::To(String& str)
{
	byte* macs = (byte*)&Value;

	char ss[18];
	memset(ss, 0, ArrayLength(ss));
	sprintf(ss, "%02X-%02X-%02X-%02X-%02X-%02X", macs[0], macs[1], macs[2], macs[3], macs[4], macs[5]);
	str.DeepSet(ss, 0);

	return str;
}
