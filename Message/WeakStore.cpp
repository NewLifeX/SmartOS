#include "WeakStore.h"

// 初始化
WeakStore::WeakStore(int magic, byte* ptr, uint len) : Data(0x40)
{
	Magic	= magic;

	// 如果指定了外部空间，则使用外部空间，否则使用内部默认分配的空间
	if(ptr && len > 0) Data.Set(ptr, len);

	if(magic && Check())
	{
		debug_printf("首次上电  ");
		Init();
	}
}

// 检查并确保初始化，返回原来是否已初始化
bool WeakStore::Check()
{
	int mg = *(int*)Data.GetBuffer();
	return mg == Magic;
}

void WeakStore::Init()
{
	debug_printf("初始化 0x%08X，幻数 0x%08X\r\n", Data.GetBuffer(), Magic);
	Data.Copy((byte*)&Magic, 4);
}

byte& WeakStore::operator[](int i) const
{
	return Data[i];
}
