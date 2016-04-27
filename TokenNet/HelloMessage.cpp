#include "Time.h"
#include "HelloMessage.h"

#include "Message\BinaryPair.h"

// 请求：2版本 + S类型 + S名称 + 8本地时间 + 6本地IP端口 + S支持加密算法列表
// 响应：2版本 + S类型 + S名称 + 8本地时间 + 6对方IP端口 + 1加密算法 + N密钥
// 错误：0xFE/0xFD + 1协议 + S服务器 + 2端口

// 初始化消息，各字段为0
HelloMessage::HelloMessage() : Cipher(1), Key(0)
{
	Version		= Sys.Version;

	ushort code = _REV16(Sys.Code);
	Type		= Buffer(&code, 2).ToHex();
	Name		= Sys.Company;
	LocalTime	= Time.Now().TotalMicroseconds();
	Cipher[0]	= 1;

	Protocol	= 2;
	Port		= 0;
}

HelloMessage::HelloMessage(const HelloMessage& msg) : MessageBase(msg), Cipher(1), Key(0)
{
	Version		= msg.Version;
	Type		= msg.Type;
	Name		= msg.Name;
	LocalTime	= msg.LocalTime;
	EndPoint	= msg.EndPoint;
	Cipher.Copy(0, msg.Cipher, 0, msg.Cipher.Length());
	Key.Copy(0, msg.Key, 0, msg.Key.Length());

	Protocol	= msg.Protocol;
	Port		= msg.Port;
	Server		= msg.Server;
}

// 从数据流中读取消息
bool HelloMessage::Read(Stream& ms)
{
	BinaryPair bp(ms);

	if(Reply && Error)
	{
		ErrCode	= ms.ReadByte();
		//ErrCode	= bp.GetValue("ErrorCode");
		if(ErrCode == 0xFE || ErrCode == 0xFD)
		{
			Protocol	= ms.ReadByte();
			Server		= ms.ReadString();
			Port		= ms.ReadUInt16();
			VisitToken	= ms.ReadString();

			return false;
		}
		else if(ErrCode < 0x80)
		{
			ErrMsg		= ms.ReadString();
		}
		else
		{
			debug_printf("无法识别错误码 0x%02X \r\n", ErrCode);

			return false;
		}

		return true;
	}

	/*Version		= ms.ReadUInt16();
	Type		= ms.ReadString();
	Name		= ms.ReadString();
	LocalTime	= ms.ReadUInt64();
	EndPoint	= ms.ReadArray(6);*/
	
	/*auto bs	= bp.Get("Ver");
	if(bs.Length()) Version	= bs.ToUInt16();
	
	bs	= bp.Get("Type");
	if(bs.Length()) Type	= bs.ToString();
	
	bs	= bp.Get("Name");
	if(bs.Length()) Name	= bs.ToString();
	
	bs	= bp.Get("Time");
	if(bs.Length()) LocalTime	= bs.ToUInt64();
	
	bs	= bp.Get("EndPoint");
	if(bs.Length()) EndPoint	= bs.ToString();
	
	bs	= bp.Get("Cipher");
	if(bs.Length()) Cipher	= bs.ToString();
	
	bs	= bp.Get("Key");
	if(bs.Length()) Key		= bs;*/
	
	bp.Get("Ver", Version);
	bp.Get("Type", Type);
	bp.Get("Name", Name);
	bp.Get("Time", LocalTime);
	bp.Get("EndPoint", EndPoint);
	bp.Get("Cipher", Cipher);
	bp.Get("Key", Key);

	/*if(!Reply)
	{
		Cipher	= ms.ReadArray();
	}
	else
	{
		Cipher[0]	= ms.ReadByte();
		// 读取数组前，先设置为0，避免实际长度小于数组长度
		Key.SetLength(0);
		Key		= ms.ReadArray();
	}*/

	return false;
}

// 把消息写入数据流中
void HelloMessage::Write(Stream& ms) const
{
	/*ms.Write(Version);
	ms.WriteArray(Type);
	ms.WriteArray(Name);
	ms.Write(LocalTime);
	ms.Write(EndPoint.ToArray());

	if(!Reply)
	{
		ms.WriteArray(Cipher);
	}
	else
	{
		ms.Write(Cipher[0]);
		ms.WriteArray(Key);
	}*/

	BinaryPair bp(ms);

	bp.Set("Ver", Version);
	bp.Set("Type", Type);
	bp.Set("Name", Name);
	bp.Set("Time", LocalTime);
	bp.Set("EndPoint", EndPoint);
	bp.Set("Cipher", Cipher);
	bp.Set("Key", Key);
}

#if DEBUG
// 显示消息内容
String& HelloMessage::ToStr(String& str) const
{
	str += "握手";
	if(Reply) str += '#';

	if(Reply && Error)
	{
		if(ErrCode == 0xFE || ErrCode == 0xFD)
		{
			//str	+= ' ';
			str	+= (ErrCode == 0xFD) ? " 临时跳转 " : " 永久跳转 ";

			if(Protocol == ProtocolType::Tcp)
				str += "Tcp://";
			else if(Protocol == ProtocolType::Udp)
				str += "Udp://";

			str = str + Server + ":" + Port;

			return str;
		}
	}

	str.Format(" Ver=%04X", Version);
	str = str + " " + Type + " " + Name + " ";

	DateTime dt;
	dt.ParseUs(LocalTime);
	str += dt;

	str = str + " " + EndPoint;

	str = str + " Cipher[" + Cipher.Length() + "]=";
	for(int i=0; i<Cipher.Length(); i++)
	{
		str += Cipher[i] + ' ';
	}

	if(Reply) str = str + " Key=" + Key;

	return str;
}
#endif
