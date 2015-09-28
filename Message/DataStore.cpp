#include "DataStore.h"

// 初始化
DataStore::DataStore() : Areas(0)
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
	for(int i=0; i<Areas.Length(); i++)
	{
		Area& ar = Areas[i];
		if(ar.Size == 0) break;

		// 数据操作口只认可完整的当前区域
		if(ar.Port && offset <= ar.Offset && offset + size >= ar.Offset + ar.Size)
		{
			if(mode == 1)
			{
				if(ar.Port->Write(&Data[ar.Offset]) <= 0) return false;
			}
			else if(mode == 0 || mode == 2)
			{
				if(ar.Port->Read(&Data[ar.Offset]) <= 0) return false;
			}
		}
		if(ar.Hook && ar.Contain(offset, size))
		{
			if(!ar.Hook(offset, size, mode)) return false;
		}
	}

	return true;
}

// 注册某一块区域的读写钩子函数
void DataStore::Register(uint offset, uint size, Handler hook)
{
	Area& ar = Areas.Push();

	ar.Offset	= offset;
	ar.Size	= size;
	ar.Hook	= hook;
}

void DataStore::Register(uint offset, IDataPort& port)
{
	Area& ar = Areas.Push();

	ar.Offset	= offset;
	ar.Size	= port.Size();
	ar.Port	= &port;
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

/****************************** 数据操作接口 ************************************/

ByteDataPort::~ByteDataPort()
{
	Sys.RemoveTask(_tid);
}

int ByteDataPort::Write(byte* data)
{
	byte cmd = *data;
	if(cmd == 0xFF) return Read(data);

	debug_printf("控制0x%02X ", cmd);
	switch(cmd)
	{
		case 1:
			debug_printf("打开");
			OnWrite(1);
			break;
		case 0:
			debug_printf("关闭");
			OnWrite(0);
			break;
		case 2:
			debug_printf("反转");
			OnWrite(!OnRead());
			break;
		default:
			break;
	}
	switch(cmd>>4)
	{
		// 普通指令
		case 0:
			// 关闭所有带有延迟效果的指令
			Next = 0xFF;
			break;
		// 开关闪烁
		case 1:
			debug_printf("闪烁 %d 秒", cmd - 0x10);
			OnWrite(!OnRead());
			Next = cmd;
			StartAsync((cmd - 0x10) * 1000);
			break;
		// 开关闪烁（毫秒级）
		case 2:
			debug_printf("闪烁 %d 毫秒", (cmd - 0x20) * 100);
			OnWrite(!OnRead());
			Next = cmd;
			StartAsync((cmd - 0x20) * 100);
			break;
		// 打开，延迟一段时间后关闭
		case 4:
		case 5:
		case 6:
		case 7:
			debug_printf("延迟 %d 秒关闭", cmd - 0x40);
			//OnWrite(1);
			Next = 0;
			StartAsync(cmd - 0x40);
			break;
		// 关闭，延迟一段时间后打开
		case 8:
		case 9:
		case 10:
		case 11:
			debug_printf("延迟 %d 秒打开", cmd - 0x80);
			//OnWrite(0);
			Next = 1;
			StartAsync(cmd - 0x80);
			break;
	}
#if DEBUG
	//Name.Show(true);
	//debug_printf(" %s\r\n", Name);
	//Show(true);
	Object* obj = dynamic_cast<Object*>(this);
	if(obj)
	{
		debug_printf(" ");
		obj->Show(true);
	}
	else
		debug_printf("\r\n");
#endif

	return Read(data);
}

void ByteDataPort::AsyncTask(void* param)
{
	ByteDataPort* dp = (ByteDataPort*)param;
	byte cmd = dp->Next;
	if(cmd != 0xFF) dp->Write(&cmd);
}

void ByteDataPort::StartAsync(int ms)
{
	if(!_tid) _tid = Sys.AddTask(AsyncTask, this, -1, -1, "定时开关");
	Sys.SetTask(_tid, true, ms * 1000);
}
