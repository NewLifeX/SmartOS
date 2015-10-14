
#ifndef __IRCODING_H__
#define __IRCODING_H__

#include "Sys.h"
#include "..\Drivers\FlashFace.h"

/*	
	红外编码的处理，
	读写Flash内的编码，与应用交互
*/


class IRcoding
{
public:
	IRcoding(FlashFace * flash = NULL);
	~IRcoding();
	
private:
	FlashFace * _Medium;
};

#endif
