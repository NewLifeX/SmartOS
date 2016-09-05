#include "HelloMessage.h"

#include "Message\BinaryPair.h"

// 请求：2版本 + S类型 + S名称 + 8本地时间 + 6本地IP端口 + S支持加密算法列表
// 响应：2版本 + S类型 + S名称 + 8本地时间 + 6对方IP端口 + 1加密算法 + N密钥
// 错误：0xFE/0xFD + 1协议 + S服务器 + 2端口

// 初始化消息，各字段为0
HelloMessage::HelloMessage() : Cipher(1), Key(0), Cookie(0)
{
	Version		= Sys.Ver;

	ushort code = _REV16(Sys.Code);
	Type		= Buffer(&code, 2).ToHex();
	Name		= Sys.Company;
	LocalTime	= DateTime::Now().TotalMs();
	Cipher[0]	= 1;
	ErrCode		= 0;
	//Protocol	= 17;
	//Port		= 0;
	Uri.Type	= NetType::Udp;
}

HelloMessage::HelloMessage(const HelloMessage& msg) : MessageBase(msg), Cipher(1), Key(0), Cookie(1)
{
	Version		= msg.Version;
	Type		= msg.Type;
	Name		= msg.Name;
	LocalTime	= msg.LocalTime;
	EndPoint	= msg.EndPoint;
	Cipher.Copy(0, msg.Cipher, 0, msg.Cipher.Length());
	Key.Copy(0, msg.Key, 0, msg.Key.Length());
	Cookie.Copy(0, msg.Cookie, 0, msg.Cookie.Length());

	ErrCode		= msg.ErrCode;
	//Protocol	= msg.Protocol;
	//Port		= msg.Port;
	//Server		= msg.Server;
	Uri			= msg.Uri;
}

// 从数据流中读取消息
bool HelloMessage::Read(Stream& ms)
{
	BinaryPair bp(ms);

	if(Reply && Error)
	{
		bp.Get("ErrorCode", ErrCode);
		if(ErrCode == 0xFE || ErrCode == 0xFD)
		{
			ByteArray uri;
			if (bp.Get("Redirect", uri))
			{
				Stream urims(uri);
				BinaryPair uribp(urims);

				byte type;
				uribp.Get("Type", type);			// 服务NetType  17 为UDP
				Uri.Type	= (NetType)type;
				if (Uri.Type == NetType::Unknown) Uri.Type = NetType::Udp;	// 避免 unknown 出现
				uribp.Get("Host", Uri.Host);
				uint uintPort;							// 服务端 Port 为 int 类型
				uribp.Get("Port", uintPort);
				Uri.Port = uintPort ;
			}
			bp.Get("Cookie", Cookie);

			return false;
		}
		else if(ErrCode < 0x80)
		{
			bp.Get("ErrorMessage", ErrMsg);
		}
		else
		{
			debug_printf("无法识别错误码 0x%02X \r\n", ErrCode);

			return false;
		}

		return true;
	}

	bp.Get("Ver", Version);
	bp.Get("Type", Type);
	bp.Get("Name", Name);
	bp.Get("Time", LocalTime);
	bp.Get("Protocol", Protocol);
	bp.Get("EndPoint", EndPoint);
	bp.Get("Cipher", Cipher);
	bp.Get("Key", Key);
	bp.Get("Cookie", Cookie);

	return false;
}

// 把消息写入数据流中
void HelloMessage::Write(Stream& ms) const
{
	BinaryPair bp(ms);
	if(ErrCode != 0) bp.Set("ErrorCode", ErrCode);
	if(Cookie.Length())bp.Set("Cookie", Cookie);
	bp.Set("Ver", Version);
	bp.Set("Type", Type);
	bp.Set("Name", Name);
	bp.Set("Time", LocalTime);
	bp.Set("Protocol", Protocol);
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

			/*if(Protocol == NetType::Tcp)
				str += "Tcp://";
			else if(Protocol == NetType::Udp)
				str += "Udp://";

			str = str + Server + ":" + Port;*/
			str	+= Uri;
		}
		else
		{
			str	= str + " 错误码=" + ErrCode + " " + ErrMsg;
		}

		return str;
	}

	str.Format(" Ver=%04X", Version);
	str = str + " " + Type + " " + Name + " ";

	DateTime dt;
	dt.ParseMs(LocalTime);
	str += dt;

	str = str + " " + EndPoint;

	if(Cipher)	str = str + " Cipher=" + Cipher;

	if(Reply)	str = str + " Key=" + Key;

	return str;
}
#endif


TokenPingMessage::TokenPingMessage()
{
	LocalTime = DateTime::Now().TotalMs();
}

TokenPingMessage::TokenPingMessage(const TokenPingMessage& msg)
{
	LocalTime = msg.LocalTime;
}

// 从数据流中读取消息
bool TokenPingMessage::Read(Stream& ms)
{
	BinaryPair bp(ms);
	bp.Get("Time", LocalTime);

	return true;
}

// 把消息写入数据流中
void TokenPingMessage::Write(Stream& ms) const
{
	BinaryPair bp(ms);
	bp.Set("Time", LocalTime);
}

#if DEBUG
// 显示消息内容
String& TokenPingMessage::ToStr(String& str) const
{
	str += "Ping";
	if (Reply) str += '#';

	DateTime dt;
	dt.ParseMs(LocalTime);
	str += dt;

	return str;
}
#endif
