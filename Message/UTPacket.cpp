#include "UTPacket.h"


UTPacket*  UTPacket::Current = nullptr;

UTPacket::UTPacket()
{
	Ports.Clear();
	Client = nullptr;
	Current = this;
}

UTPacket::UTPacket(TokenClient * client)
{
	Ports.Clear();
	Set(client);
	Current = this;
}

UTPacket::~UTPacket()
{
	Ports.Clear();
	if (AycUptTaskId)Sys.RemoveTask(AycUptTaskId);
	if (CacheA)delete CacheA;

	Current = nullptr;
}

bool UTPacket::Set(TokenClient * client)
{
	// 设置迟了会直接启用
	if (client)Client = client;
	if (Ports.Count() && Client)Client->Register("UTPacket", &UTPacket::PressTMsg, this);

	return true;
}
// 异步发送
void UTPacket::AsynUpdata()
{
	if (!CacheA->Position())return;
	Buffer data(CacheA->GetBuffer(), CacheA->Position());
	Client->Invoke("UTPacket", data);	// 发送
	CacheA->SetPosition(0);				// 清空缓冲区
	Sys.SetTask(AycUptTaskId, false);	// 不考虑双缓冲区，即不考虑发的时候有数据进缓冲区！！
										// 关闭减少系统调度
}

bool UTPacket::Send(Buffer &packet)
{
	if (!Client || !packet.Length())return false;
	if (packet.Length() > 256)Client->Invoke("UTPacket", packet);		// 超过256 直接发送 减少拷贝的问题
	if (!CacheA)CacheA = new MemoryStream();
	if (!AycUptTaskId)AycUptTaskId = Sys.AddTask(&UTPacket::AsynUpdata, this, 3000, 3000, "UTPacket");

	CacheA->Write(packet);
	Sys.SetTask(AycUptTaskId, true, 3000);			// 默认3秒后发送
	if (CacheA->Position() > 1024)AsynUpdata();		// 超过1k直接调度 因为上面的256限制 所以这里最大1280 不超标
	// Sys.SetTask(AycUptTaskId, true, 0);			// 直接调用更直接 减少系统调度损耗时间
													// 此处的优化会造成不能在中断内调用，如果有问题改用Sys.SetTask
	return true;
}


bool UTPacket::AndPort(byte id, UTPort* port)
{
	Ports.Add((uint)id, port);
	return true;
}


bool UTPacket::PressTMsg(const Pair& args, Stream& result)
{
	auto buff = args.Get("Data");			// 引用源数据区，后面使用要小心，不能修改任何值。
	if (buff.Length() < (int)sizeof(PacketHead))return false;

	auto head = (PacketHead*)buff.GetBuffer();
	void * tail = head + buff.Length();		// 结尾位置
	UTPort* port = nullptr;

	while (head < tail)
	{
		if ((uint)head->Next() > (uint)tail)break;	// 要么越界，要么数据包错

		MemoryStream resms;
		if (!Ports.TryGetValue((uint)head->PortID, port) || !port)		// 获取端口
		{
			PacketHead err;							// 获取失败 返回错误信息
			err.Seq = head->Seq;
			err.Length = 0;
			err.Error.ErrorCode = NoPort;			// 无端口的错误   需要定义类型
			err.PortID = head->PortID;
			err.Type = head->Type;
			Buffer errbuf(&err, sizeof(PacketHead));
			result.Write(errbuf);
			continue;
		}

		Buffer packet(head, head->Length + sizeof(PacketHead));			// 封装Packet
		port->DoFunc(packet, resms);									// 执行 返回相应的信息
		result.Write(Buffer(resms.GetBuffer(), resms.Position()));		// 写入响应

		head = (PacketHead*)head->Next();								// 下一个Packet
		port = nullptr;
	}
	return true;
}
