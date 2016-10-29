#include "Sys.h"

#include "Can.h"

Can::Can(CAN index, Mode_TypeDef mode, int remap)
{
    _index	= index;
    Mode	= mode;
    Remap	= remap;

	_Tx		= nullptr;
	_Rx		= nullptr;
}

void Can::Open()
{
	OnOpen();
}

Can::~Can()
{
	auto tx	= (int*)_Tx;
	delete tx;

	auto rx	= (int*)_Rx;
	delete rx;
}
