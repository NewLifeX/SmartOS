#ifndef _Sys_H_
#define _Sys_H_

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

#include "Core\Type.h"
#include "Core\Buffer.h"
#include "Core\Array.h"
#include "Core\ByteArray.h"
#include "Core\SString.h"
#include "Core\Stream.h"
#include "Core\DateTime.h"
#include "Core\Version.h"
#include "Core\List.h"
#include "Core\Dictionary.h"
#include "Core\Delegate.h"

/* 引脚定义 */
#include "Platform\Pin.h"

// 强迫内联
#define _force_inline __attribute__( ( always_inline ) ) __INLINE

extern "C"
{
#if defined(DEBUG)
	#define debug_printf printf
#else
	#define debug_printf(format, ...)
#endif
}

#ifdef DEBUG

void assert_failed2(cstring msg, cstring file, unsigned int line);
#define assert(expr, msg) ((expr) ? (void)0 : assert_failed2(msg, (const char*)__FILE__, __LINE__))

#else

#define assert(expr, msg) ((void)0)

#endif

#if defined(BOOT) || defined(APP)
struct HandlerRemap
{
	Func pUserHandler;
	void* Reserved1;
	void* Reserved2;
	void* Reserved3;
};
extern struct HandlerRemap StrBoot;
#endif

class SystemConfig;

// 系统类
class TSys
{
public:
    byte	MessagePort;// 消息口，默认0表示USART1

    uint	Clock;  	// 系统时钟
    uint	CystalClock;// 晶振时钟

	cstring	Name;		// 系统名称
	cstring	Company;	// 系统厂商
	ushort	Code;		// 产品代码
	ushort	Ver;		// 系统版本
    byte	ID[12];		// 芯片ID。
    ushort	DevID;		// MCU编码。低字设备版本，高字子版本
    ushort	RevID;		// MCU编码。低字设备版本，高字子版本
    uint	CPUID;		// CPUID
    ushort	FlashSize;	// 芯片Flash容量。
    ushort	RAMSize;	// 芯片RAM容量

	const SystemConfig*	Config;	// 系统设置

    TSys();				// 构造函数

	void InitClock();	// 初始化系统时钟
    void Init();     	// 初始化系统
	void ShowInfo() const;
	uint HeapBase() const;	// 堆起始地址，前面是静态分配内存
	uint StackTop() const;	// 栈顶，后面是初始化不清零区域
	void SetStackTop(uint addr);

	UInt64	Ms() const;		// 系统启动后的毫秒数
	uint	Seconds() const;	// 系统绝对当前时间，秒

    void Sleep(uint ms) const; // 毫秒级延迟
    void Delay(uint us) const; // 微秒级延迟
	typedef void (*FuncU32)(uint param);
	FuncU32 OnSleep;

	bool CheckMemory() const;

	// 延迟异步重启
	void Reboot(int msDelay = 0) const;

	// 系统跟踪
	void InitTrace(void* port) const;
	void Trace(int times = 1) const;

private:
	// 重启系统
    void Reset() const;
	void OnInit();
	void OnShowInfo() const;
	void OnStart();

public:
	// 创建任务，返回任务编号。dueTime首次调度时间ms，period调度间隔ms，-1表示仅处理一次
	uint AddTask(Action func, void* param, int dueTime = 0, int period = 0, cstring name = nullptr) const;
	template<typename T>
	uint AddTask(void(T::*func)(), T* target, int dueTime = 0, int period = 0, cstring name = nullptr)
	{
		return AddTask(*(Action*)&func, target, dueTime, period, name);
	}
	void RemoveTask(uint& taskid) const;
	// 设置任务的开关状态，同时运行指定任务最近一次调度的时间，0表示马上调度
	bool SetTask(uint taskid, bool enable, int msNextTime = -1) const;
	// 改变任务周期
	bool SetTaskPeriod(uint taskid, int period) const;

	bool Started;
	void Start();	// 开始系统大循环
};

extern TSys Sys;		// 创建一个全局的Sys对象  会在main函数之前执行构造函数（！！！！！）

// 系统设置
class SystemConfig
{
public:
	// 操作系统
	char	Name[16];	// 系统名称。如：SmartOS-M3-10x
	uint	Ver;		// 系统版本。系统内

	// 硬件
	uint	HardVer;	// 硬件版本

	// 应用软件
	ushort	Type;		// 产品种类
	uint	AppVer;		// 产品版本
	char	Serial[16];	// 授权码
	char	Product[16];// 产品名称
	char	Company[16];// 公司

	char	DevID[16];	// 设备编码
	char	Server[32];	// 服务器。重置后先尝试厂商前端，再尝试原服务器
	char	Token[32];	// 访问服务器的令牌

	uint	Expire;		// 有效期。1970以来的秒数。
	
	ushort	Checksum;	// 校验
};

void EnterCritical();
void ExitCritical();

//extern uint32_t __REV(uint32_t value);
//extern uint32_t __REV16(uint16_t value);

uint _REV(uint value);
ushort _REV16(ushort value);

// 智能IRQ，初始化时备份，销毁时还原
// SmartIRQ相当霸道，它直接关闭所有中断，再也没有别的任务可以跟当前任务争夺MCU
class SmartIRQ
{
public:
	SmartIRQ(bool enable = false);
	~SmartIRQ();

private:
	uint _state;
};

#if DEBUG
// 函数栈。
// 进入函数时压栈函数名，离开时弹出。便于异常时获取主线程调用列表
class TraceStack
{
public:
	TraceStack(cstring name);
	~TraceStack();

	static void Show();
};

#define TS(name) TraceStack __ts(name)

#else

#define TS(name) ((void)0)

#endif

#endif //_Sys_H_

/*
v3.2.2016.0517	核心类独立到目录Core，平台无关，系统无关

v3.1.2015.1108	增加系统配置存储模块，增加电源管理模块

v3.0.2015.0806	增强系统调度器，支持无阻塞多任务调度

v2.8.2014.0927	完成微网通讯架构，封装消息协议，串口及nRF24L01+测试通过
v2.7.2014.0919	支持抢占式多线程调度
v2.6.2014.0823	平台文件独立，接管系统初始化控制权
v2.5.2014.0819	增加堆栈溢出检测模块，重载new/delete实现，仅Debug有效
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
