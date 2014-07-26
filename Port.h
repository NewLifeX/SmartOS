#ifndef _Port_H_
#define _Port_H_

#include "Sys.h"

#ifdef STM32F10X
	#include "stm32f10x_gpio.h"
#else
	#include "stm32f0xx_gpio.h"
#endif

// 读取委托
typedef void (*IOReadHandler)(Pin , bool );

// 端口类
class Port
{
public:
    typedef enum
    {
      Mode_IN   = 0x00, /*!< GPIO Input Mode              */
      Mode_OUT  = 0x01, /*!< GPIO Output Mode             */
      Mode_AF   = 0x02, /*!< GPIO Alternate function Mode */
      Mode_AN   = 0x03  /*!< GPIO Analog In/Out Mode      */
    }Mode_TypeDef;

    typedef enum
    { 
      Speed_10MHz = 1,
      Speed_2MHz, 
      Speed_50MHz
    }Speed_TypeDef;
    
    typedef enum
    {
      PuPd_NOPULL = 0x00,
      PuPd_UP     = 0x01,
      PuPd_DOWN   = 0x02
    }PuPd_TypeDef;

    GPIO_TypeDef* Group;// 针脚组
    ushort PinBit;       // 组内引脚位。每个引脚一个位

    Mode_TypeDef Mode;  // 模式
    bool IsOD;          // 是否开漏输出
    Speed_TypeDef Speed;// 速度
    PuPd_TypeDef PuPd;  // 上拉下拉电阻
    
    bool Invert;        // 是否倒置输入输出

    Port(Pin pin);      // 单一引脚初始化
    Port(Pin pins[]);    // 用一组引脚来初始化，引脚组GPIOx由第一个引脚决定，请确保所有引脚位于同一组GPIOx
    Port(GPIO_TypeDef* group, ushort pinbit = GPIO_Pin_All);

    void Config();    // 确定配置,确认用对象内部的参数进行初始化
    void Write(bool value); // 按位值写入
    bool Read(); // 读取本组所有引脚，任意脚为true则返回true，主要为单一引脚服务
    void WriteGroup(ushort value);   // 整组写入
    ushort ReadGroup();    // 整组读取

    static void Set(Pin pin, Mode_TypeDef mode = Mode_AF, bool isOD = true, Speed_TypeDef speed = Speed_2MHz, PuPd_TypeDef pupd = PuPd_NOPULL);
    static void SetInput(Pin pin, bool isFloating = true, Speed_TypeDef speed = Speed_2MHz);
    static void SetOutput(Pin pin, bool isOD = true, Speed_TypeDef speed = Speed_50MHz);
    static void SetAlternate(Pin pin, bool isOD = true, Speed_TypeDef speed = Speed_2MHz);	// 复用输出功能设置
    static void SetAnalog(Pin pin, bool isOD = true, Speed_TypeDef speed = Speed_2MHz);			// 模拟量输入输出

    static void Write(Pin pin, bool value);
    static bool Read(Pin pin);
    static void Register(Pin pin, IOReadHandler handler);			// 申请引脚中断托管
    static void SetShakeTime(byte ms);			//设置按键去抖动时间

    // 辅助函数
    static GPIO_TypeDef* IndexToGroup(byte index);
    static byte GroupToIndex(GPIO_TypeDef* group);
    static ushort IndexToBits(byte index);
    static byte BitsToIndex(byte bits); // 最低那一个位的索引
};

#endif //_Port_H_
