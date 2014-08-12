#ifndef __ADC_H__
#define __ADC_H__

#include "Pin.h"
#include "Port.h"		
#include "DMA.h"

/* 
Analog-to-digital converter
用规则通道  使用DMA实现数据迁移

STM32F103内部ADC
	3个 12位 主次逼近型模拟数字转换器（ADC）。   共用
	18个通道，分别为16个外部16路 和 2个内部信号（温度和PVD）
	各个通道的A/D转换可以是单次、连续、扫描或者是间断模式执行
	转换结果可以设置对齐格式  左对齐/右对齐
	
	PVD模拟看门狗特性允许应用程序检测输入电压是否合格。
	
*/



class AnalogInput : public	Port	
{
	
public :
	typedef enum
    {	// 16路 引脚对应 和两路内部
	   Channel_0 = PA0,
	   Channel_1 = PA1,
	   Channel_2 = PA2,
	   Channel_3 = PA3,
	   Channel_4 = PA4,
	   Channel_5 = PA5,
	   Channel_6 = PA6,
	   Channel_7 = PA7,
	   Channel_8 = PA8,
	   Channel_9 = PA9,
	   Channel_10 = PC0,
	   Channel_11 = PC1,
	   Channel_12 = PC2,
	   Channel_13 = PC3,
	   Channel_14 = PC4,
	   Channel_15 = PC5,
	   Vrefint	  = 0x80,	// PVD电源检测
	   Temperature =0x81	// IC内部温度
	}ADC_Channel;
	
	// 用规则通道  使用DMA实现数据迁移
	AnalogInput(ADC_Channel line);
	int Read();
	
private :
	int _ChannelNum;

	static Pin PinList[18];
	static int AnalogValue[18];
	static void RegisterDMA(ADC_Channel lime);	// 注册DMA信息

protected:
	virtual void OnConfig();
	virtual bool OnReserve(Pin pin, bool flag);
	virtual ~AnalogInput();

};


#endif
