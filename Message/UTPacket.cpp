#include "UTPacket.h"


UTRet UTCom::DoFunc(Buffer & line, MemoryStream & ret)
{
	return Good;
}


typedef struct
{
	byte PortID;	// 端口号
	byte Type;		// 数据类型
	ushort Length;	// 数据长度
	void * Next()const { return (void*)(&Length + Length + 1); };
}PacketHead;


bool UTPacket::PressTMsg(const BinaryPair& args, Stream& result)
{
	byte data[2048];
	Buffer buff(data,sizeof(data));
	args.Get("Data", buff);

	PacketHead * head = (PacketHead*)buff.GetBuffer();





	return true;
}

