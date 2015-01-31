#ifndef __ADC_H__
#define __ADC_H__

#include "Sys.h"
//#include "DMA.h"

/*
Analog-to-digital converter
用规则通道  使用DMA实现数据迁移

STM32F103内部ADC
	3个 12位 主次逼近型模拟数字转换器（ADC）。   共用
	18个通道，分别为16个外部16路 和 2个内部信号（温度和PVD）
	各个通道的A/D转换可以是单次、连续、扫描或者是间断模式执行
	转换结果可以设置对齐格式  左对齐/右对齐

	PVD模拟看门狗特性允许应用程序检测输入电压是否合格。
	Port
*/

class ADConverter  //: public	AnalogPort   //有两个通道不在引脚上  不用继承的好
{
private:
	ADC_TypeDef*	_ADC;	// 当前中断线的引用

public :
	byte	Line;		// 中断线 1/2/3
	byte	Count;		// 通道个数
	uint	Channel;	// 使用哪些通道，每个通道一位
#ifdef STM32F1
	ushort	Data[18];	// 存放数据
#elif defined(STM32F0)
	ushort	Data[19];	// 存放数据
#endif

	ADConverter(byte line = 1, uint channel = 0);

	void Add(Pin pin);
	void Remove(Pin pin);
	void Open();
	ushort Read(Pin pin);
	ushort ReadTempSensor();
	ushort ReadVrefint();
	ushort ReadVbat();
};
#endif
