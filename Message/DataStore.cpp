#include "Kernel\Sys.h"

#include "DataStore.h"

//#define DS_DEBUG DEBUG
#define DS_DEBUG 0
#if DS_DEBUG
#define ds_printf debug_printf
#else
#define ds_printf(format, ...)
#endif

class Area
{
public:
	uint	Offset;
	uint	Size;

	DataStore::Handler	Hook;
	IDataPort*	Port;

	Area();
	bool In(uint start, uint len);
	bool Any(uint start, uint len);

	friend bool operator==(const Area& a1, const Area& a2)
	{
		return a1.Offset == a2.Offset && a1.Size == a2.Size;
	}
};

// 初始化
DataStore::DataStore()
{
	Strict = true;
}

// 写入数据
int DataStore::Write(uint offset, const Buffer& bs)
{
	int size = bs.Length();
	if (size == 0) return 0;

	int realOffset = offset - VirAddrBase;

	//起始位置越界
	auto len = Data.Length();
	if (realOffset >= len) return -1;

	// 数据越界
	if (Strict && realOffset + size > len) size = len - realOffset;

	// 从数据区读取数据
	uint rs = Data.Copy(realOffset, bs, 0, size);
	if (rs == 0) return rs;

	// 执行钩子函数
	if (!OnHook(realOffset, rs, true)) return -1;

	return rs;
}

// 读取数据
int DataStore::Read(uint offset, Buffer& bs)
{
	int size = bs.Length();
	if (size == 0) return 0;

	int realOffset = offset - VirAddrBase;
	int remain = Data.Length() - realOffset;

	// 检查是否越界
	// if(Strict && realOffset + size > Data.Length()) return -1;
	// 只要起始位置在区间内都读，数据超长就返回能返回的！
	if (Strict && remain <= 0) return -1;		// 起始地址越界直接返回

	// 最大只能读取这么多
	if (size > remain) size = remain;

	// 执行钩子函数
	if (!OnHook(realOffset, size, false)) return -1;

	// 从数据区读取数据
	return bs.Copy(0, Data, realOffset, size);
}

bool DataStore::OnHook(uint offset, uint size, bool write)
{
	for (int i = 0; i < Areas.Count(); i++)
	{
		auto ar = *(Area*)Areas[i];
		if (ar.Size == 0) continue;

		// 数据操作口只认可完整的当前区域
		if (ar.Port && ar.In(offset, size))
		{
			if (write)
			{
				if (ar.Port->Write(&Data[ar.Offset]) <= 0) return false;
			}
			else
			{
				if (ar.Port->Read(&Data[ar.Offset]) <= 0) return false;
			}
		}
		// 钩子函数不需要完整区域，只需要部分匹配即可
		if (ar.Hook && ar.Any(offset, size))
		{
			if (!ar.Hook(ar.Offset, ar.Size, write)) return false;
		}
	}

	return true;
}

// 注册某一块区域的读写钩子函数
void DataStore::Register(uint offset, uint size, Handler hook)
{
	auto ar = new Area();
	ar->Offset = offset;
	ar->Size = size;
	ar->Hook = hook;

	Areas.Add(ar);
}

void DataStore::Register(uint offset, IDataPort& port)
{
	auto ar = new Area();
	ar->Offset = offset;
	ar->Size = port.Size();
	ar->Port = &port;

	Areas.Add(ar);
}

Area::Area()
{
	Offset = 0;
	Size = 0;
	Hook = nullptr;
}

// 参数是读命令里面的偏移和大小
bool Area::In(uint start, uint len)
{
	// 数据操作口只认可完整的当前区域
	return Offset >= start && Offset + Size <= start + len;
}

bool Area::Any(uint start, uint len)
{
	// 只要搭边就算数
	// start  <= A < start + len
	// Offset <= B < Offset + Size

	// A左边小于B右边，且A右边大于B左边
	//return start < Offset + Size && start + len > Offset;

	// 不是A右边小于等于B左边，且不是A左边大于等于B右边
	return !(Offset >= start + len || Offset + Size <= start);
}

/****************************** 数据操作接口 ************************************/

ByteDataPort::ByteDataPort()
{
	Busy = false;
}

ByteDataPort::~ByteDataPort()
{
	Sys.RemoveTask(_tid);
}

int ByteDataPort::Write(byte* data)
{
	Busy = false;

	byte cmd = *data;
	if (cmd == 0xFF) return Read(data);

	ds_printf("控制0x%02X ", cmd);
	switch (cmd)
	{
	case 1:
		ds_printf("打开");
		OnWrite(1);
		break;
	case 0:
		ds_printf("关闭");
		OnWrite(0);
		break;
	case 2:
		ds_printf("反转");
		OnWrite(!OnRead());
		break;
	default:
		break;
	}
	int s = 0;
	switch (cmd >> 4)
	{
		// 普通指令
	case 0:
		// 关闭所有带有延迟效果的指令
		Next = 0xFF;
		break;
		// 开关闪烁
	case 1:
		s = cmd - 0x10;
		ds_printf("闪烁 %d 秒", s);
		OnWrite(!OnRead());
		Next = cmd;
		StartAsync(s * 1000);
		break;
		// 开关闪烁（毫秒级）
	case 2:
		s = (cmd - 0x20) * 100;
		ds_printf("闪烁 %d 毫秒", s);
		OnWrite(!OnRead());
		Next = cmd;
		StartAsync(s);
		break;
		// 打开，延迟一段时间后关闭
	case 4:
	case 5:
	case 6:
	case 7:
		s = cmd - 0x40;
		ds_printf("延迟 %d 秒关闭", s);
		//OnWrite(1);
		Next = 0;
		StartAsync(s * 1000);
		break;
		// 关闭，延迟一段时间后打开
	case 8:
	case 9:
	case 10:
	case 11:
		s = cmd - 0x80;
		ds_printf("延迟 %d 秒打开", s);
		//OnWrite(0);
		Next = 1;
		StartAsync(s * 1000);
		break;
	}
#if DS_DEBUG
	//Name.Show(true);
	//ds_printf(" %s\r\n", Name);
	//Show(true);
	/*Object* obj = dynamic_cast<Object*>(this);
	if(obj)
	{
		ds_printf(" ");
		obj->Show(true);
	}
	else*/
	ds_printf("\r\n");
#endif

	return Read(data);
}

void ByteDataPort::Flush(int second)
{
	int cmd = 0x10 + second;
	Write(cmd);
}

void ByteDataPort::FlushMs(int ms)
{
	int cmd = 0x20 + ms;
	Write(cmd);
}

void ByteDataPort::DelayOpen(int second)
{
	int cmd = 0x80 + second;
	Write(cmd);
}

void ByteDataPort::DelayClose(int second)
{
	int cmd = 0x40 + second;
	Write(cmd);
}

void ByteDataPort::AsyncTask(void* param)
{
	ByteDataPort* dp = (ByteDataPort*)param;
	dp->Busy = false;
	byte cmd = dp->Next;
	if (cmd != 0xFF) dp->Write(&cmd);
}

void ByteDataPort::StartAsync(int ms)
{
	// 不允许0毫秒的异步事件
	if (!ms) return;

	Busy = true;
	if (!_tid) _tid = Sys.AddTask(AsyncTask, this, -1, -1, "定时开关");
	Sys.SetTask(_tid, true, ms);
}

int DataOutputPort::OnWrite(byte data)
{
	if (!Port) return 0;

	Port->Write(data > 0);
	return OnRead();
};

byte DataOutputPort::OnRead()
{
	if (!Port) return 0;

	return Port->Read() ? 1 : 0;
};

String DataOutputPort::ToString() const
{
	String str;
	if (!Port) return str;

	return Port->ToString();
}

int DataInputPort::Write(byte* data)
{
	if (!Port) return 0;

	return Read(data);
};

int DataInputPort::Read(byte* data)
{
	if (!Port) return 0;

	*data = Port->Read() ? 1 : 0;
	return Size();
};

String DataInputPort::ToString() const
{
	String str;
	if (!Port) return str;

	return Port->ToString();
}
