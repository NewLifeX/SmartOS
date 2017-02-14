#ifndef _BaseBoard_H_
#define _BaseBoard_H_

#include "Sys.h"
#include "Device\RTC.h"

/*struct
{
	byte ClksCount;
	byte OldIdx;	// 初始值无效
	UInt64 LastClkTim;
	void Init()
	{
		ClksCount = 0;
		OldIdx = 0xff;
		LastClkTim = 0;
	};
}ClickStr;*/

class BaseBoard
{
public:
	BaseBoard();
	void Init(ushort code, cstring name, COM message = COM1);
	//初始化按键重启
	void InitReboot();
	//初始化断电重置
	void InitRestore();
};

#endif
