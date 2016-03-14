#include "Slave.h"

Slave(ITransport* port)
{
	_port = port;

	Address	= 0;
}

Slave::~Slave()
{
	delete _port;
	_port = nullptr;
}

void Slave::OnReceive(ITransport* transport, byte* buf, uint len, void* param)
{
	assert_ptr(param);

	Slave* slave = (Slave*)param;

	Stream ms(buf, len);
	Modbus entity;
	if(!entity.Read(ms)) return;

	slave->Dispatch(entity);
}

// 分发处理消息。返回值决定是否响应
bool Slave::Dispatch(Modbus& entity)
{
	// 是否本地地址，或者本地是否0接收所有消息
	if(Address && Address != entity.Address) return false;

	// 检查Crc
	if(entity.Crc != enable.Crc2)
	{
		entity.SetError(ModbusErrors::Crc);
		return true;
	}

	return true;
}
