#ifndef _Port_H_
#define _Port_H_

#include "Sys.h"

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

    static void Set(Pin pin, Mode_TypeDef = Mode_AF, bool isOD = true, Speed_TypeDef speed = Speed_2MHz, PuPd_TypeDef pupd = PuPd_NOPULL);
    static void SetInput(Pin pin, bool isFloating = true, Speed_TypeDef speed = Speed_2MHz);
    static void SetOutput(Pin pin, bool isOD = true, Speed_TypeDef speed = Speed_50MHz);
    static void SetAlternate(Pin pin, bool isOD = true, Speed_TypeDef speed = Speed_2MHz);
    static void SetAnalog(Pin pin, bool isOD = true, Speed_TypeDef speed = Speed_2MHz);

    static void Write(Pin pin, bool value);
    static bool Read(Pin pin);
    static void Register(Pin pin, IOReadHandler handler);
    static void SetShakeTime(byte ms);
};

#endif //_Port_H_
