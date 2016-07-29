#include "UTPacket.h"


void UTCom::DoFunc(Buffer & packet, MemoryStream & ret)
{
}

bool UTPacket::PressTMsg(const BinaryPair& args, Stream& result)
{
	Buffer buff = args.Get("Data");			// 引用源数据区，后面使用要小心，不能修改任何值。
	if (buff.Length() < sizeof(PacketHead))return false;

	PacketHead * head = (PacketHead*)buff.GetBuffer();
	void * tail = head + buff.Length();		// 结尾位置
	UTPort * port = nullptr;

	while (head<tail)
	{
		if ((uint)head->Next() >(uint)tail)break;					// 要么越界，要么数据包错

		MemoryStream resms;
		if (!Ports.TryGetValue((uint)head->PortID, port) || !port)		// 获取端口
		{
			PacketHead err;				// 获取失败 返回错误信息
			err.Seq = head->Seq;
			err.Length = 0;
			err.Error = NoPort;			// 无端口的错误   需要定义类型
			err.PortID = head->PortID;
			err.Type = head->Type;
			Buffer errbuf(&err, sizeof(PacketHead));
			result.Write(errbuf);
			continue;
		}

		Buffer packet(head, head->Length + sizeof(PacketHead));
		port->DoFunc(packet, resms);									// 执行 返回相应的信息
		result.Write(Buffer(resms.GetBuffer(), resms.Position()));		// 写入响应

		head = (PacketHead*)head->Next();
		port = nullptr;
	}
	return true;
}

#if DEBUG
String& UTPacket::ToStr(String& str) const
{
	return str;
}
#endif 
