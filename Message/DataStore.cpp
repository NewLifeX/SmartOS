#include "DataStore.h"

// 初始化
DataStore::DataStore()
{
	Strict	= true;
}

// 写入数据
int DataStore::Write(uint offset, ByteArray& bs)
{
	uint size = bs.Length();
	if(size == 0) return 0;

	// 检查是否越界
	if(Strict && offset + size > Data.Length()) return -1;

	// 执行钩子函数
	if(!OnHook(offset, size, 0)) return -1;

	// 从数据区读取数据
	uint rs = Data.Copy(bs, offset);

	// 执行钩子函数
	if(!OnHook(offset, size, 1)) return -1;

	return rs;
}

// 读取数据
int DataStore::Read(uint offset, ByteArray& bs)
{
	uint size = bs.Length();
	if(size == 0) return 0;

	// 检查是否越界
	if(Strict && offset + size > Data.Length()) return -1;

	// 执行钩子函数
	if(!OnHook(offset, size, 2)) return -1;

	// 从数据区读取数据
	return bs.Copy(Data.GetBuffer() + offset, size);
}

bool DataStore::OnHook(uint offset, uint size, int mode)
{
	for(int i=0; i<ArrayLength(Areas); i++)
	{
		Area& ar = Areas[i];
		if(ar.Offset == 0 && ar.Size == 0) break;

		if(ar.Hook && ar.Contain(offset, size))
		{
			if(!ar.Hook(offset, size, mode)) return false;
			
			// 只命中第一个钩子，缩短时间
			break;
		}
	}
	
	return true;
}

// 注册某一块区域的读写钩子函数
void DataStore::Register(uint offset, uint size, Handler hook)
{
	// 找一个空位
	int i=0;
	for(i=0; i<ArrayLength(Areas); i++)
	{
		if(Areas[i].Offset == 0 && Areas[i].Size == 0) break;
	}
	if(i >= ArrayLength(Areas))
	{
		debug_printf("数据存储区的读写钩子函数已满\r\n");
		return;
	}

	Areas[i].Offset	= offset;
	Areas[i].Size	= size;
	Areas[i].Hook	= hook;
}

DataStore::Area::Area()
{
	Offset	= 0;
	Size	= 0;
	Hook	= NULL;
}

bool DataStore::Area::Contain(uint offset, uint size)
{
	return (Offset <= offset && offset <= Offset + Size ||
			Offset >= offset && Offset <= offset + size);
}
