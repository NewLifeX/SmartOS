#include "UTPort.h"


UTPort::UTPort()
{
	Seq = 0;
	Name = nullptr;
}


enum UTComOrder : byte
{
	Config = 1,		// 数据使用BinaryPair来处理
	Open = 2,		// 数据长度为0 
	Write = 3,		// 数据使用Buffer  直接就是要发送的数据
	Read = 4,		// 收到2字节的长度，返回2字节实际长度+数据   实际读到的长度小于等于命令长度
	Close = 5,		// 数据产股为0
	// ChgName = 6,	// 改变名称，暂时不用
};


UTCom::UTCom()
{
	Port = nullptr;
	State = 0;
}

void UTCom::DoFunc(Buffer & packet, MemoryStream & ret)
{
	PacketHead * head = (PacketHead*)packet.GetBuffer();

	PacketHead err;										// 准备一个错误的容器
	Buffer((void*)&err, sizeof(err)) = (void*)head;		// 拷贝一份
	err.Error.ErrorCode = Good;
	err.Length = 0;

	UTComOrder orr = (UTComOrder)head->Type;
	switch (orr)
	{
	case Config:
	{
		if (State = 0)CreatePort();

		Stream data(&head->Length + sizeof(head->Length), head->Length);
		BinaryPair cfg(data);
		if (!ComConfig(cfg))
		{
			err.Error.ErrorCode = CfgError;
			ret.Write(Buffer(&err, sizeof(err)));
			return;
		}
		State = 2;
	}
		break;
	case Open:
	{
		if (State < 2)		// 没有初始化完成是不许OPEN的
		{
			err.Error.ErrorCode = CmdError;
			ret.Write(Buffer(&err, sizeof(err)));
			return;
		}
		Port->Open();
		//Port->Register()

	}
		break;
	case Write:
		break;
	case Read:
		break;
	case Close:
		break;
	default:
		break;
	}

}

bool UTCom::ComConfig(BinaryPair & data)
{

	return true;
}






