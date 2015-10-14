
#include "IRcoding.h"


IRcoding::IRcoding(FlashFace * flash)
{
	assert_param2(flash, "必须给出存储介质");
	_Medium = flash;
}

IRcoding::~IRcoding()
{
}


