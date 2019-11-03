#include "NPA_700B_015A.h"

NPA_700B_015A::NPA_700B_015A()
{
	this->IIC = nullptr;
	this->Address = 0X51;
}

void NPA_700B_015A::Init()
{
	debug_printf("\r\nAT24CXX::Init Address=0x%02X \r\n", Address);

	this->IIC->SubWidth = 1;
	this->IIC->Address = this->Address;
	this->IIC->Open();
}

int NPA_700B_015A::Read()
{
	byte buf1, buf2;
	int buf;
	buf1 = 0;
	buf2 = 0;
	this->IIC->Start();
	this->IIC->WriteByte(Address);
	if (!this->IIC->WaitAck(true))
	{
		return  -1;
	}
	buf1 = this->IIC->ReadByte();
	this->IIC->Ack(true);
	buf2 = this->IIC->ReadByte();
	this->IIC->Ack(false);
	this->IIC->Stop();
	buf = (buf1 << 8) | buf2;
	return buf;
}
//读取大气压值
float NPA_700B_015A::ReadP()
{
	return this->Read() * 0.007278646;
}
