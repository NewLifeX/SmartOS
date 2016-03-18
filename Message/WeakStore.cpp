#include "WeakStore.h"

// 初始化
WeakStore::WeakStore(const char* magic, byte* ptr, uint len) : Data(0x40)
{
	Magic		= magic;
	MagicLength	= 0;

	// 如果指定了外部空间，则使用外部空间，否则使用内部默认分配的空间
	if(ptr && len > 0) Data.Set(ptr, len);

	if(magic)
	{
		auto p = magic;
		while(*p++) MagicLength++;
		if(!Check())
		{
			debug_printf("首次上电  ");
			Init();
		}
	}
}

// 检查并确保初始化，返回原来是否已初始化
bool WeakStore::Check()
{
	assert(Magic, "未指定幻数");

	auto mg = (const char*)Data.GetBuffer();
	//return strcmp(mg, Magic) == 0;
	auto p = Magic;
	for(int i=0; i<MagicLength; i++)
	{
		if(*mg++ != *p++) return false;
	}
	return true;
}

void WeakStore::Init()
{
	assert(Magic, "未指定幻数");

	debug_printf("初始化 0x%08X，幻数 %s\r\n", Data.GetBuffer(), Magic);
	Data.Clear();
	Data.Copy(0, (byte*)Magic, MagicLength);
}

// 重载索引运算符[]，定位到数据部分的索引。
byte WeakStore::operator[](int i) const
{
	return Data[MagicLength + i];
}

byte& WeakStore::operator[](int i)
{
	return Data[MagicLength + i];
}
