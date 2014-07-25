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

/* 引脚定义 */
typedef ushort			Pin;
#include "Pin.h"

// 系统类
class TSys
{
public:
    bool Debug;  // 是否调试
	uint Clock;  // 系统时钟
    uint CystalClock;    // 晶振时钟
    byte MessagePort;    // 消息口，默认0表示USART1
    uint ID[3];     // 芯片ID
    uint FlashSize; // 芯片Flash容量
    uint MCUID;     // MCU编码。低字设备版本，高字子版本
    bool IsGD;      // 是否GD芯片

    TSys();
    void Init();     // 初始化系统
    void Sleep(uint ms); // 毫秒级延迟
    void Delay(uint us); // 微秒级延迟
    void DisableInterrupts();    // 关闭中断
    void EnableInterrupts();     // 打开中断
};

extern TSys Sys;

#endif //_Sys_H_
