#ifndef _SYSTEM_H_
#define _SYSTEM_H_

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

/* 串口定义 */
#define COM1 0
#define COM2 1
#define COM3 2
#define COM4 3
#define COM5 4
#define COM_NONE 0xFF

/*Spi定义*/
//SPI1..这种格式与st库冲突  
#define SPI_1	0
#define SPI_2	1
#define SPI_3	2
#define SPI_NONE 0XFF

/* 引脚定义 */
typedef ushort			Pin;
#include "Pin.h"

#endif //_SYSTEM_H_
