#include "TokenBoard.h"

TokenBoard::TokenBoard()
{
	Client = nullptr;

	Data = nullptr;
	Size = 0;
}

void* TokenBoard::InitData(void* data, int size)
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
void TokenBoard::Write(uint offset, byte data)
{
	auto client = Client;
	if (!client) return;

	client->Store.Write(offset, data);
	client->ReportAsync(offset, 1);
}

/******************************** Token ********************************/

void TokenBoard::InitClient(bool useLocal)
{
	if (Client) return;

	// 初始化配置区
	InitConfig();

	// 创建客户端
	auto tc = TokenClient::Create("udp://smart.wslink.cn:33333", Buffer(Data, Size));
	if (useLocal) tc->UseLocal();

	Client = tc;
}

void TokenBoard::Register(uint offset, IDataPort& dp)
{
	if (!Client) return;

	auto& ds = Client->Store;
	ds.Register(offset, dp);
}

void TokenBoard::Register(uint offset, uint size, Handler hook)
{
	if (!Client) return;

	auto& ds = Client->Store;
	ds.Register(offset, size, hook);
}

void TokenBoard::Invoke(const String& ation, const Buffer& bs)
{
	if (!Client) return;

	Client->Invoke(ation, bs);

}

void TokenBoard::Restore()
{
	if (!Client) return;

	if (Client) Client->Reset("按键重置");
}

/*int TokenBoard::GetStatus()
{
	if (!Client) return 0;

	return Client->Status;
}*/

void TokenBoard::OnLongPress(InputPort* port, bool down)
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
