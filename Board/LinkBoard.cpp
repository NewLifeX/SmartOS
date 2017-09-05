#include "LinkBoard.h"

#include "Drivers\W5500.h"
#include "Drivers\Esp8266\Esp8266.h"

LinkBoard::LinkBoard()
{
	Client = nullptr;

	Data = nullptr;
	Size = 0;
}

void* LinkBoard::InitData(void* data, int size)
{
	// 启动信息
	auto hot = &HotConfig::Current();
	hot->Times++;

	data = hot->Next();
	if (hot->Times == 1)
	{
		Buffer ds(data, size);
		ds.Clear();
		ds[0] = size;
	}

	Data = data;
	Size = size;

#if DEBUG
	debug_printf("数据区[%d]：", hot->Times);
	Buffer(Data, Size).Show(true);
#endif

	return data;
}

// 写入数据区并上报
void LinkBoard::Write(uint offset, byte data)
{
	auto client = Client;
	if (!client) return;

	client->Store.Write(offset, data);
	client->ReportAsync(offset, 1);
}

/******************************** Link ********************************/

void LinkBoard::InitClient(cstring server)
{
	if (Client) return;

	if (!server) server = "tcp://feifan.link:2233";

	// 创建客户端
	auto tc = LinkClient::Create(server, Buffer(Data, Size));

	Client = tc;
}

void LinkBoard::Register(uint offset, IDataPort& dp)
{
	if (!Client) return;

	auto& ds = Client->Store;
	ds.Register(offset, dp);
}

void LinkBoard::Register(uint offset, uint size, Handler hook)
{
	if (!Client) return;

	auto& ds = Client->Store;
	ds.Register(offset, size, hook);
}

void LinkBoard::OnLongPress(InputPort* port, bool down)
{
	if (down) return;

	debug_printf("Press P%c%d Time=%d ms\r\n", _PIN_NAME(port->_Pin), port->PressTime);

	auto client = Client;
	if (!client) return;

	ushort time = port->PressTime;
	if (time >= 5000 && time < 10000)
	{
		client->Reset("按键重置");
	}
	else if (time >= 3000)
	{
		client->Reboot("按键重启");
		Sys.Reboot(1000);
	}
}
