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
//#define true            1
//#define false           0

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

/* 串口定义 */
#define COM1 0
#define COM2 1
#define COM3 2
#define COM4 3
#define COM5 4
#define COM_NONE 0xFF

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
    uint CystalClock;	// 晶振时钟
    byte MessagePort;	// 消息口，默认0表示USART1
#ifdef STM32F0XX
    byte ID[4];			// 芯片ID
#else
    byte ID[12];		// 芯片ID
#endif
    ushort FlashSize;	// 芯片Flash容量
    ushort RAMSize;		// 芯片RAM容量
    uint CPUID;			// CPUID
    ushort DevID;		// MCU编码。低字设备版本，高字子版本
    ushort RevID;		// MCU编码。低字设备版本，高字子版本
    bool IsGD;			// 是否GD芯片

    TSys();					//构造函数
    virtual ~TSys();//析构函数

    void Init();     // 初始化系统
	void ShowInfo();

    void Sleep(uint ms); // 毫秒级延迟
    void Delay(uint us); // 微秒级延迟

	bool CheckMemory();

    void Reset();   // 重启系统
    void (*OnError)(int code);  // 系统出错时引发
    Func OnStop;

    // CRC32校验
    uint Crc(const void* rgBlock, int len, uint crc = 0);

private:

	// 任务类
	class Task
	{
	public:
		int ID;			// 编号
		Action Callback;// 回调
		void* Param;	// 参数
		int Period;		// 周期us
		ulong NextTime;	// 下一次执行时间
	};

	Task* _Tasks[32];
	int _TaskCount;
	bool _Running;
	
public:
	// 创建任务，返回任务编号。dueTime首次调度时间us，period调度间隔us，-1表示仅处理一次
	uint AddTask(Action func, void* param, uint dueTime = 0, int period = 0);
	void RemoveTask(uint taskid);
	void Start();	// 开始系统大循环
	void Stop();
};

extern TSys Sys;		//创建一个全局的Sys对象  会在main函数之前执行构造函数（！！！！！）

// 强迫内联
#define force_inline __attribute__( ( always_inline ) ) __INLINE

extern "C"
{
#ifdef DEBUG

#define debug_printf printf

#else

__inline void debug_printf( const char *format, ... ) {}

#endif  // !defined(BUILD_RTM)
}

#include "Time.h"
#include "Interrupt.h"


#endif //_Sys_H_

/*
v2.4.2014.0811	实现系统多任务调度，一次性编译测试通过，多任务小灯例程4k
				实现以太网精简协议TinyIP，ARP/ICMP/TCP/UDP，混合网络例程7.5k
				增加看门狗、定时器模块
v2.3.2014.0806	使用双栈增加稳定性，增加RTM优化编译，核心函数强制内联，自动堆栈越界检查
v2.2.2014.0801	增加引脚保护机制，避免不同模块使用相同引脚导致冲突而难以发现错误
v2.1.2014.0728	增加中断管理模块，全面接管中断向量表，支持动态修改中断函数，支持多中断共用中断函数。F0需配置RAM从0x200000C0开始
v2.0.2014.0725	使用C++全新实现SmartOS，支持系统时钟、IO、USART、日志、断言、Spi、NRF24L01、SPIFlash、CAN、Enc28j60，GD32超频

v1.3.2014.0624	增加Spi模块和NRF24L01+模块的支持
v1.2.2014.0604	支持GD32芯片
v1.1.2014.0513	支持F0/F1的GPIO和串口功能
v1.0.2014.0506	建立嵌入式系统框架SmartOS，使用纯C语言实现
*/
