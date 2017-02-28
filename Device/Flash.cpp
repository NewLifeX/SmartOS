#include "Flash.h"

#define FLASH_DEBUG DEBUG

Flash::Flash()
{
	Size	= Sys.FlashSize << 10;
	Block	= 0x400;
    Start	= 0x8000000;
	ReadModifyWrite	= true;
	XIP		= true;

	OnInit();
}
