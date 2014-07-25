#ifndef _Sys_H_
#define _Sys_H_

#include <stdio.h>
#include "stm32.h"

/* 类型定义 */
typedef char            sbyte;
typedef unsigned char   byte;
typedef unsigned short  ushort;
typedef unsigned int    uint;
typedef char*           string;
//typedef unsigned char   bool;
#define true            1
#define false           0

/*Spi定义*/
//SPI1..这种格式与st库冲突  
#define SPI_1	0
#define SPI_2	1
#define SPI_3	2
#define SPI_NONE 0XFF

/* 引脚定义 */
typedef ushort			Pin;
#include "Pin.h"

// 系统类
class TSys
{
public:
    bool Debug;  // 是否调试
	uint Clock;  // 系统时钟
#if GD32F1
    uint CystalClock;    // 晶振时钟
#endif
    byte MessagePort;    // 消息口，默认0表示USART1
    uint ID[3];      // 芯片ID
    uint FlashSize;  // 芯片Flash容量
    void Init();     // 初始化系统
    void Sleep(uint ms); // 毫秒级延迟
    void Delay(uint us); // 微秒级延迟
    void DisableInterrupts();    // 关闭中断
    void EnableInterrupts();     // 打开中断
};

extern TSys Sys;

#endif //_Sys_H_
