
#include "TokenDataMessage.h"
#include "Message\BinaryPair.h"

TokenDataMessage::TokenDataMessage() :Data(0)
{
	ID		= 0;
	Start	= 0;
	Size	= 0;
}

TokenDataMessage::TokenDataMessage(const TokenDataMessage& msg)
{
	ID		= msg.ID;
	Start	= msg.Start;
	Size	= msg.Size;
	Data	= msg.Data;
}

// 从数据流中读取消息
bool TokenDataMessage::Read(Stream& ms)
{
	BinaryPair bp(ms);
	bp.Get("ID", ID);
	bp.Get("Start", Start);
	bp.Get("Size", Size);
	bp.Get("Data", Data);

	return true;
}

// 把消息写入数据流中
void TokenDataMessage::Write(Stream& ms) const
{
	BinaryPair bp(ms);
	if(ID) bp.Set("ID", ID);
	bp.Set("Start", Start);
	if(Size) bp.Set("Size", Size);
	if(Data.Length()) bp.Set("Data", Data);
}

// 读取数据
bool TokenDataMessage::ReadData(const DataStore& ds)
{
	ByteArray bs(Size);
	auto ds2 = (DataStore*)&ds;
	if (ds2->Read(Start, bs) != -1)return ReadData(bs);

	// 出错返回false
	bs.SetLength(0);
	return false;
}

// 读取数据
bool TokenDataMessage::ReadData(const Buffer& bs)
{
	if(!Size) return false;

	TS("TokenDataMessage::ReadData");

	//int remain	= bs.Length() - Start;
	//if(remain < 0) return false;

	//int len	= Size;
	//if(len > remain) len = remain;
	//if (len > 0) Data = bs;// .Sub(Start, len);

	if (bs.Length() == 0)return false;
	Data = bs;
	return true;
}

// 写入数据
bool TokenDataMessage::WriteData(DataStore& ds, bool withData)
{
	TS("TokenDataMessage::WriteData");

	ds.Write(Start, Data);

	return true;
}

// 写入数据
bool TokenDataMessage::WriteData(Buffer& bs, bool withData)
{
	TS("TokenDataMessage::WriteData");

	bs.Copy(Data, Start);

	return true;
}

#if DEBUG
// 显示消息内容
String& TokenDataMessage::ToStr(String& str) const
{
	str += "DateMsg";
	if (Reply) str += '#';
	if(ID) str	= str + " ID:" + ID;
	str	= str + " Start:" + Start;
	if(Size) str	= str + " Size:" + Size;

	if(Data)
	{
		str	= str + " Data[" + Data.Length() + "]=";
		str	+= Data;
	}

	return str;
}
#endif
