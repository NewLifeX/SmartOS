
#include "Config.h"

#include "TokenNet\Token.h"
#include "Security\RC4.h"

#include "DeviceBody.h"
#include "TinyNet\DataMessage.h"

DeviceBody::DeviceBody(BodyManagement* mgmt)
{
	if (mgmt)Mgmt = mgmt;

	// 直接使用 Device 类的数据区
	Store.Data.Set(DevInfo._Store, sizeof(DevInfo._Store));
}

bool DeviceBody::SetSecDaStor(uint addr, byte * buf, int len)
{
	if (addr < Store.Data.Length() || addr > 0x8000000)
	{
		debug_printf("DeviceBody::SetSecDaStor 地址设置不合法\r\n");
		return false;
	}
	if (buf == nullptr || len == 0 || len > 2048)return false;
	Store2 = new DataStore();
	Store2->Data.Set(buf, len);
	Store2->VirAddrBase = addr;
	return true;
}

bool DeviceBody::SetSecDaStor(DataStore * store)
{
	if (store->VirAddrBase + store->Data.Length() > 0x8000000 || store->VirAddrBase < Store.Data.Length())
	{
		debug_printf(" DeviceBody::SetSecDaStor 虚拟地址不正确\r\n");
	}
	Store2 = store;
	return true;
}

// byte * DeviceBody::GetData()
// {
// 	if (DevInfo._Store == Store.Data.GetBuffer)
// 		return Store.Data.GetBuffer();
// 	return nullptr;
// }

void DeviceBody::Open()
{
	if (Opened)return;
	debug_printf("DeviceBody::Open()  ");

	if (Mgmt==nullptr)
	{
		debug_printf("BodyMgmt 错误\r\n");
		return;
	}

	// 调用子类 Open 函数
	if (!OnOpen())return;


	if (DevInfo.Kind == 0)
	{
		debug_printf("请明确设备类型\r\n");
		return;
	}

	// auto pMgmt = DevicesManagement::Current;
	// if (!pMgmt)
	// {
	// 	debug_printf("没有 DevicesManagement\r\n");
	// 	return;
	// }
	// // 把自己装入设备列表里面  并且一直在线
	// pMgmt->PushDev(&DevInfo);
	// pMgmt->OnlineAlways.Push(&DevInfo);

	debug_printf("\r\n");
	Opened = true;
}

void DeviceBody::Close()
{
	if (!Opened)return;
	debug_printf("DeviceBody::Close()  ");


	// 调用子类 Open 函数
	if (!OnClose())return;

	// // 把自己装入设备列表里面  并且一直在线
	// auto pMgmt = DevicesManagement::Current;
	// pMgmt->DeleteDev(DevInfo.Kind);

	debug_printf("\r\n");
	Opened = false;
}

void DeviceBody::OnWrite(const TokenMessage & msg)
{
	if (msg.Reply) return;
	if (msg.Length < 2) return;
	TS("DeviceBody::OnWrite");

	TokenMessage rs;
	rs.Code = msg.Code;
	rs.Reply = true;
	rs.State = msg.State;
	//rs.Seq = msg.Seq;

	auto ms = rs.ToStream();
	DataMessage dm(msg, &ms, true);

	bool rt = true;
	if (dm.Offset < 64)
		rt = dm.WriteData(Store, false);
	else if (dm.Offset < 128)
	{
		dm.Offset -= 64;
		//rt	= dm.WriteData(dv.GetConfig(), false);
	}
	else if(Store2 && dm.Offset > Store2->VirAddrBase)
	{
		rt = dm.WriteData(*Store2, false);
	}

	rs.Error = !rt;
	rs.Length = ms.Position();

	// Reply(rs);
}

void DeviceBody::OnRead(const TokenMessage & msg)
{
	if (msg.Reply) return;
	if (msg.Length < 2) return;

	TokenMessage rs;
	rs.Code = msg.Code;
	rs.Reply = true;
	rs.State = msg.State;
	//rs.Seq = msg.Seq;

	auto ms = rs.ToStream();

	DataMessage dm(msg, &ms, true);

	bool rt = true;
	if (dm.Offset < 64)
		rt = dm.ReadData(Store);
	else if (dm.Offset < 128)
	{
		//dm.Offset -= 64;
		//Buffer bs(Cfg, Cfg->Length);
		//rt = dm.ReadData(bs);
	}
	else if (Store2 && dm.Offset > Store2->VirAddrBase)
	{
		rt = dm.ReadData(*Store2);
	}

	rs.Error = !rt;
	rs.Length = ms.Position();

	// Reply(rs);
}

DeviceBody * BodyManagement::FindBody(byte id) const
{
	if (id == 0) return nullptr;
	for (int i = 0; i < Bodys.Count(); i++)
	{
		auto dv	= (DeviceBody*)Bodys[i];
		if (dv->DevInfo.Address == id) return dv;
	}
	return nullptr;
}

byte GetAddr(TokenMessage & msg)
{
	Stream ms = msg.ToStream();
	if (ms.Length > 1)
		return ms.ReadByte();
	return 0;
}

void BodyManagement::Send(TokenMessage & msg)
{
	byte id = GetAddr(msg);
	if (id == 0)return;
	auto body = FindBody(id);
	switch (msg.Code)
	{
	case 5:
	case 15:
		body->OnRead(msg);
		break;
	case 6:
	case 16:
		body->OnWrite(msg);
		break;

	default:
		break;
	}
}

void BodyManagement::Report(TokenMessage & msg)
{
}
