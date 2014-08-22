#ifndef __ADC_H__
#define __ADC_H__

#include "Sys.h"
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
	Port
*/

class ADConverter  //: public	AnalogPort   //有两个通道不在引脚上  不用继承的好
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
	
	ADConverter();
	ADConverter(ADC_Channel line);
	
	void OnConfig();
	static void StartConverter();
	
//	bool AddLine(ADC_Channel line);
	int Read();
	void ReallyChannel(ADC_Channel ParameterChannel); //ADC_Channel上的通道转化为真实的通道
	
private :
	byte _ADC_group;	// 通道挂在那个ADC上
	byte _ReallyChannel;
	int _ChannelNum;	// 在_AnalogValue[18];中的第几个数  可能被修改 
						// 使用前验证_isChangeEvent 标志参数
	static void RegisterDMA(ADC_Channel lime);	// 注册DMA信息
	static byte isSmall();
protected:
	virtual ~ADConverter();
//	virtual void OnConfig();
//	virtual bool OnReserve(Pin pin, bool flag);

};



class ADC_Stru
{
public:
	ADC_InitTypeDef adc_intstr;		// 初始化ADC所使用的函数
	volatile int _AnalogValue[6]; 	// 每个ADC最多分摊到6个通道进行转换

	ADC_Stru(byte ADC_group);		// 构造函数
};


#endif
