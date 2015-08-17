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
	if(Strict)
	{
		if(offset + size > Data.Length()) return -1;
	}

	// 执行钩子函数
	for(int i=0; i<ArrayLength(Areas); i++)
	{
		Area& ar = Areas[i];
		if(ar.Offset == 0 && ar.Size == 0) break;

		if(ar.Writing)
		{
			if (ar.Offset <= offset && offset <= ar.Offset + ar.Size ||
				ar.Offset >= offset && ar.Offset <= offset + size)
			{
				if(!ar.Writing(*this, offset, size, bs)) return -1;
			}
		}
	}

	// 从数据区读取数据
	uint rs = Data.Copy(bs, offset);

	// 执行钩子函数
	for(int i=0; i<ArrayLength(Areas); i++)
	{
		Area& ar = Areas[i];
		if(ar.Offset == 0 && ar.Size == 0) break;

		if(ar.Wrote)
		{
			if (ar.Offset <= offset && offset <= ar.Offset + ar.Size ||
				ar.Offset >= offset && ar.Offset <= offset + size)
			{
				ar.Wrote(*this, offset, size, bs);
			}
		}
	}

	return rs;
}

// 读取数据
int DataStore::Read(uint offset, ByteArray& bs)
{
	uint size = bs.Length();
	if(size == 0) return 0;

	// 检查是否越界
	if(Strict)
	{
		if(offset + size > Data.Length()) return -1;
	}

	// 执行钩子函数
	for(int i=0; i<ArrayLength(Areas); i++)
	{
		Area& ar = Areas[i];
		if(ar.Offset == 0 && ar.Size == 0) break;

		if(ar.Read)
		{
			if (ar.Offset <= offset && offset <= ar.Offset + ar.Size ||
				ar.Offset >= offset && ar.Offset <= offset + size)
			{
				if(!ar.Read(*this, offset, size, bs)) return -1;
			}
		}
	}

	// 从数据区读取数据
	return bs.Copy(Data.GetBuffer() + offset, size);
}

// 注册某一块区域的读写钩子函数
void DataStore::Register(uint offset, uint size, Handler writing, Handler wrote, Handler read)
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
	Areas[i].Writing= writing;
	Areas[i].Wrote	= wrote;
	Areas[i].Read	= read;
}

DataStore::Area::Area()
{
	Offset	= 0;
	Size	= 0;
	Writing	= NULL;
	Wrote	= NULL;
	Read	= NULL;
}
