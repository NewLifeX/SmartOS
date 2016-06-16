#include "EspUdp.h"

/******************************** Udp ********************************/

EspUdp::EspUdp(Esp8266& host, byte idx)
	: EspSocket(host, ProtocolType::Udp, idx)
{

}

bool EspUdp::SendTo(const Buffer& bs, const IPEndPoint& remote)
{
	//!!! ESP8266有BUG，收到数据后，远程地址还是乱了，所以这里的远程地址跟实际可能不一致
	if(remote == Remote) return Send(bs);

	if(!Open()) return false;

	String cmd	= "AT+CIPSEND=";
	cmd = cmd + _Index + ',' + bs.Length();

	// 加上远程IP和端口
	cmd	= cmd + ",\"" + remote.Address + "\"";
	cmd	= cmd + ',' + remote.Port;
	cmd	+= "\r\n";

	return SendData(cmd, bs);
}

bool EspUdp::OnWriteEx(const Buffer& bs, const void* opt)
{
	auto ep = (IPEndPoint*)opt;
	if(!ep) return OnWrite(bs);

	return SendTo(bs, *ep);
}
