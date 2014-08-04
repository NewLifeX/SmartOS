#ifndef _Sys_H_
#define _Sys_H_

#include <stdio.h>
#include <string.h>
#include "stm32.h"

/* 类型定义 */
typedef char            sbyte;
typedef unsigned char   byte;
typedef unsigned short  ushort;
typedef unsigned int    uint;
typedef unsigned long long  ulong;
typedef char*           string;
//typedef unsigned char   bool;
#define true            1
#define false           0

/*
// 尚未决定是否采用下面这种类型
typedef char            SByte;
typedef unsigned char   Byte;
typedef short           Int16;
typedef unsigned short  UInt16;
typedef int             Int32;
typedef unsigned int    UInt32;
typedef long long       Int64;
typedef unsigned long long  UInt64;
typedef char*           String;
*/

/* 引脚定义 */
//typedef ushort Pin;
#include "Pin.h"

// 委托
#include "Delegate.h"

// 列表模版
#include "List.h"

// 系统类
class TSys
{
public:
    bool Debug;  // 是否调试
    bool Inited; // 是否已完成初始化
    uint Clock;  // 系统时钟
    uint CystalClock;    // 晶振时钟
    byte MessagePort;    // 消息口，默认0表示USART1
    uint ID[3];     // 芯片ID
    uint FlashSize; // 芯片Flash容量
    uint CPUID;     // CPUID
    uint MCUID;     // MCU编码。低字设备版本，高字子版本
    bool IsGD;      // 是否GD芯片

    TSys();					//构造函数
    virtual ~TSys();//析构函数

    void Init();     // 初始化系统
    void Sleep(uint ms); // 毫秒级延迟
    void Delay(uint us); // 微秒级延迟
    bool DisableInterrupts();    // 关闭中断
    bool EnableInterrupts();     // 打开中断

    void Reset();   // 重启系统
    void (*OnError)(int code);  // 系统出错时引发
    Func OnStop;
    
    // CRC32校验
    uint Crc(const void* rgBlock, int len, uint crc = 0);
};

extern TSys Sys;		//创建一个全局的Sys对象  会在main函数之前执行构造函数（！！！！！）

extern "C"
{
#if !defined(DEBUG)

#define debug_printf printf

#else

__inline void debug_printf( const char *format, ... ) {}

#endif  // !defined(BUILD_RTM)
}

#include "Time.h"
#include "Interrupt.h"


#endif //_Sys_H_
