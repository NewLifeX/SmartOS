#include "ADC.h"
#include "sys.h"
#include "Interrupt.h"
#include "Pin.h"

ADConverter::ADC_Channel  _PinList[18];  // 记录注册顺序 三个ADC 每个ADC最多分配6个通道
//volatile byte  _ADC_ChanelNum[3]={0,0,0};  // 记录每个ADC 有多少个通道在线   ->ADC_NbrOfChannel  此参数即可
volatile int _isChangeEvent=0x00000000;	//使用18位 标志通道是否被修改顺序

static ADC_TypeDef * const g_ADCs[3]= {ADC1,ADC2,ADC3};	// flash内

ADC_Stru * adc_IntStrs[3]={NULL,NULL,NULL}; // 三个ADC的类

byte ADConverter::isSmall()
{	// 避免飞出
	if(adc_IntStrs[0] == NULL)return 0;
	if(adc_IntStrs[1] == NULL)return 1;
	if(adc_IntStrs[2] == NULL)return 2;
	// 已经有对象的时候  判断line数目
	if(adc_IntStrs[1]->adc_intstr.ADC_NbrOfChannel <= adc_IntStrs[2]->adc_intstr.ADC_NbrOfChannel)
	{
		if(adc_IntStrs[0]->adc_intstr.ADC_NbrOfChannel <= adc_IntStrs[1]->adc_intstr.ADC_NbrOfChannel)return 0;
		else  return 1;
	}
	else
	{
		if(adc_IntStrs[0]->adc_intstr.ADC_NbrOfChannel <= adc_IntStrs[2]->adc_intstr.ADC_NbrOfChannel)return 0;
		else  return 2;
	}
}

void ADConverter::ReallyChannel(ADC_Channel ParameterChannel)
{
	if(ParameterChannel < PA10)
	{
		_ReallyChannel=(byte)ParameterChannel;
		return ;
	}
	if(ParameterChannel < PC6)
	{
		byte _ReallyChannel = (byte)ParameterChannel ;
		_ReallyChannel &= 0x0f;
		_ReallyChannel += 0x0a;
		return ;
	}
	if(ParameterChannel == Vrefint)
	{
		_ReallyChannel=0x10;
		return ;
	}
	if(ParameterChannel == Vrefint)
	{
		_ReallyChannel=0x10;
		//return ;
	}
}

void ADConverter:: RegisterDMA(ADC_Channel lime)	
{
}

ADConverter:: ADConverter(ADC_Channel line)
{
	assert_param( (line < PA10) || ((PC0 <= line) && (line < PC6)) || (0x80 == line)||(line == 0x81) );
	ReallyChannel(line);
	byte _ADC_group = isSmall();	// 选取负担最轻的ADC
	ADC_TypeDef * _ADC = g_ADCs[_ADC_group];	// 取得对象所对应的ADC配置寄存器地址  配置必须参数
	
	if(adc_IntStrs[_ADC_group] == NULL)		//没有初始化 ADC 初始化参数class
	{
		adc_IntStrs[_ADC_group] = new ADC_Stru(_ADC_group);	// 申请一个ADC配置类结构体class   初始化参数由构造函数完成
//		ADC_InitTypeDef * adc_intstr = & adc_IntStrs[_ADC_group]->adc_intstr;	// 取得对象
		RCC_ADCCLKConfig(RCC_PCLK2_Div6);// 时钟分频数	  需要根据时钟进行调整    ！！！
		ADC_DeInit(_ADC);	// 复位到默认值
	}
		ADC_InitTypeDef * adc_intstr = & adc_IntStrs[_ADC_group]->adc_intstr; // 取得对象
		ADC_DeInit(_ADC);	// 每次修改通道数的时候先复位	// 是否有需要 不确定   ！！！
	
		/*  ADC_ScanConvMode:
		 DISABLE	单通道模式
		 ENABLE		多通道模式（扫描模式）*/
		if(adc_intstr->ADC_NbrOfChannel == 1)
				adc_intstr->ADC_ScanConvMode=ENABLE;
	/*	ADC_NbrOfChannel:
		开启通道数   */
		adc_intstr->ADC_NbrOfChannel += 1;			//添加一个通道						
		ADC_Init(ADC1,adc_intstr);
		
		if(line >= Vrefint)	ADC_TempSensorVrefintCmd(ENABLE);	// 启用 PVD  和  温度通道
}

//bool ADConverter::AddLine(ADC_Channel line)
//{
//	assert_param( (line < PA10) || ((PC0 <= line) && (line < PC6)) || (0x80 == line)||(line == 0x81) );
//	
//	return true;
//}

void ADConverter::OnConfig()
{
}

int ADConverter::Read()
{
	return adc_IntStrs[_ADC_group]->_AnalogValue[_ChannelNum];
}

ADConverter:: ~ADConverter()
{
}

ADC_Stru::ADC_Stru(byte ADC_group)
{	
	/*	ADC_Mode   模式:
		 ADC_Mode_Independent          		独立模式         
		 ADC_Mode_RegInjecSimult           	混合的同步规则 注入同步模式        
		 ADC_Mode_RegSimult_AlterTrig    	混合的同步规则 交替触发模式          
		 ADC_Mode_InjecSimult_FastInterl  	混合同步注入 快速交替模式        
		 ADC_Mode_InjecSimult_SlowInterl   	混合同步注入 慢速交替模式      
		 ADC_Mode_InjecSimult              	注入同步模式       
		 ADC_Mode_RegSimult                	规则同步模式
		 ADC_Mode_FastInterl               	快速交替模式
		 ADC_Mode_SlowInterl              	慢速交替模式  
		 ADC_Mode_AlterTrig                	交替触发模式	*/
		adc_intstr.ADC_Mode = ADC_Mode_Independent;
		/*  ADC_ScanConvMode  转换模式:
		 DISABLE	单通道模式
		 ENABLE		多通道模式（扫描模式）*/
		adc_intstr.ADC_ScanConvMode = DISABLE;
	/*	ADC_ContinuousConvMode   是否连续转换：
		 DISABLE	单次
		 ENABLE		连续*/
		adc_intstr.ADC_ContinuousConvMode = ENABLE;	
	/*	ADC_ExternalTrigConv   触发方式:
		 ADC_ExternalTrigConv_T1_CC1                For ADC1 and ADC2 
		 ADC_ExternalTrigConv_T1_CC2                For ADC1 and ADC2 
		 ADC_ExternalTrigConv_T2_CC2                For ADC1 and ADC2 
		 ADC_ExternalTrigConv_T3_TRGO               For ADC1 and ADC2 
		 ADC_ExternalTrigConv_T4_CC4                For ADC1 and ADC2 
		 ADC_ExternalTrigConv_Ext_IT11_TIM8_TRGO    For ADC1 and ADC2 
		 ADC_ExternalTrigConv_T1_CC3                For ADC1, ADC2 and ADC3
		 ADC_ExternalTrigConv_None      	软件	For ADC1, ADC2 and ADC3
		 ADC_ExternalTrigConv_T3_CC1                For ADC3 only
		 ADC_ExternalTrigConv_T2_CC3                For ADC3 only
		 ADC_ExternalTrigConv_T8_CC1                For ADC3 only
		 ADC_ExternalTrigConv_T8_TRGO               For ADC3 only
		 ADC_ExternalTrigConv_T5_CC1                For ADC3 only
		 ADC_ExternalTrigConv_T5_CC3                For ADC3 only*/
		adc_intstr.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
	/*	ADC_DataAlign  转换结果对齐方式:
		 ADC_DataAlign_Right                        
		 ADC_DataAlign_Left     */
		adc_intstr.ADC_DataAlign = ADC_DataAlign_Right;
	/*	ADC_NbrOfChannel:
		开启通道数   */
		adc_intstr.ADC_NbrOfChannel = 0;	// 初始化为零	
}
