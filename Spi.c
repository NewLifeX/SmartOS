

#include "System.h"

#ifdef STM32F1XX
	#include "stm32f10x_spi.h"
#else
	#include "stm32f0xx_spi.h"
	#include "Pin_STM32F0.h"
#endif

#include "Pin.h"



static const  Pin  g_Spi_Pins_Map[][4]=  SPI_PINS_FULLREMAP;

/*cs引脚*/
static const Pin spi_nss[3]=
{
	//一般选取 硬件  spi nss引脚   
	PA4		,								//spi1
	PB12	,								//spi2
	PA15	,								//spi3
};

#define SPI_CS_HIGH(spi)		Sys.IO.Write(spi_nss[spi],1)
#define SPI_CS_LOW(spi)			Sys.IO.Write(spi_nss[spi],0)




#define IS_SPI(a)	   {if(3<a<0xff)return false;}



//#define SPI_NSS_PINS  {4, 28, 15} // PA4, PB12, PA15
//#define SPI_SCLK_PINS {5, 29, 19} // PA5, PB13, PB3
//#define SPI_MISO_PINS {6, 30, 20} // PA6, PB14, PB4
//#define SPI_MOSI_PINS {7, 31, 21} // PA7, PB15, PB5




#ifdef STM32F1XX
void gpio_config(const Pin  pin[])
{

	
	
}

#else

void gpio_config(const Pin pin[])
{
		//
    Sys.IO.OpenPort(pin[1], GPIO_Mode_AF, GPIO_Speed_10MHz, GPIO_OType_PP,GPIO_PuPd_NOPULL);
    Sys.IO.OpenPort(pin[2], GPIO_Mode_AF, GPIO_Speed_10MHz, GPIO_OType_PP,GPIO_PuPd_NOPULL);
    Sys.IO.OpenPort(pin[3], GPIO_Mode_AF, GPIO_Speed_10MHz, GPIO_OType_PP,GPIO_PuPd_NOPULL);
	
		//spi都在GPIO_AF_0分组内
    GPIO_PinAFConfig(_GROUP(pin[1]), _PIN(pin[1]), GPIO_AF_0);
    GPIO_PinAFConfig(_GROUP(pin[2]), _PIN(pin[2]), GPIO_AF_0);
    GPIO_PinAFConfig(_GROUP(pin[3]), _PIN(pin[3]), GPIO_AF_0);
}

#endif




bool Spi_config(int spi,int clk)
{
	//get pin
	const Pin  * p= g_Spi_Pins_Map[spi];
	
#ifdef STM32F1XX
#else
	SPI_InitTypeDef SPI_InitStructure;
	
#endif	

	IS_SPI(spi);
	gpio_config(p);
	
#ifdef STM32F1XX

  /*使能SPI1时钟*/
	switch(spi)
	{
		  case SPI_1 :RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE); break;
			case SPI_2 :RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI2, ENABLE); break;
			case SPI_3 :RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI3, ENABLE); break;
					
	}	  
  /* 拉高csn引脚，释放spi总线*/
		SPI_CS_HIGH(spi);
 
		SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex; //双线全双工
		SPI_InitStructure.SPI_Mode = SPI_Mode_Master;	 					//主模式
		SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;	 				//数据大小8位
		SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;		 				//时钟极性，空闲时为低
		SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;						//第1个边沿有效，上升沿为采样时刻
		SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;		   					//NSS信号由软件产生
		SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;  //8分频，9MHz
		SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;  				//高位在前
		SPI_InitStructure.SPI_CRCPolynomial = 7;
	switch(spi)
	{								/*				配置spi													使能spi  */	
		  case SPI_1 : SPI_Init(SPI1, &SPI_InitStructure);  SPI_Cmd(SPI1, ENABLE);	break;
			case SPI_2 : SPI_Init(SPI2, &SPI_InitStructure);  SPI_Cmd(SPI2, ENABLE);	break;
			case SPI_3 : SPI_Init(SPI3, &SPI_InitStructure);  SPI_Cmd(SPI3, ENABLE);	break;
					
	}	  
	
#else

	    /*
	*************************************************************************************
	*	SPI初始化：SPI为双线双向全双工，主SPI模式，接收发送的数据帧为8位格式			    *
	*			   空闲时时钟为高电平（W25X16选择模式3），数据在第二个时钟上升			    *
	*			   沿开始捕获（1、由于时钟空闲时为高电平，2、W25X16在上升沿时数据输入）	*
	*			   决定了是第二个时钟上升沿不是第一个时钟。SPI波特率2分频，36MHz		    *
	*			   W25X16手册说SPI时钟不超过75MHz，数据先传送高位，						*
	*************************************************************************************
	*/		
	switch(spi)
	{								
		  case SPI_1 : RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);  	break;
//			case SPI_2 : RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI2, ENABLE); 	break;
//			case SPI_3 : RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI3, ENABLE); 	break;
	}	  
	
	SPI_CS_HIGH(spi);
	
	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;  //设置SPI单向或者双向的数据模式:SPI设置为双线双向全双工
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;		//设置SPI工作模式:设置为主SPI
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;		//设置SPI的数据大小:SPI发送接收8位帧结构
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;		//选择了串行时钟的稳态:时钟悬空高
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;	//数据捕获于第二个时钟沿
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;		//NSS信号由硬件（NSS管脚）还是软件（使用SSI位）管理:内部NSS信号有SSI位控制
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;		//定义波特率预分频的值:波特率预分频值为
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;	//指定数据传输从MSB位还是LSB位开始:数据传输从MSB位开始
	SPI_InitStructure.SPI_CRCPolynomial = 7;	//CRC值计算的多项式
	
	switch(spi)
	{								/*				配置spi													使能spi  */	
		  case SPI_1 : SPI_Init(SPI1, &SPI_InitStructure);  SPI_Cmd(SPI1, ENABLE);	break;
//			case SPI_2 : SPI_Init(SPI2, &SPI_InitStructure);  SPI_Cmd(SPI2, ENABLE);	break;
//			case SPI_3 : SPI_Init(SPI3, &SPI_InitStructure);  SPI_Cmd(SPI3, ENABLE);	break;
					
	}	  
	
#endif
	
	return true;
}
	


bool Spi_Disable(int spi)
{
	
	IS_SPI(spi);
	
	SPI_CS_HIGH(spi);		//使从机释放总线

#ifdef STM32F1XX
	switch(spi)
	{								/*		失能spi  															关闭spi时钟							*/	
		  case SPI_1 :		SPI_Cmd(SPI1, DISABLE);		RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, DISABLE); break;
			case SPI_2 :		SPI_Cmd(SPI2, DISABLE);		RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI2, DISABLE); break;
			case SPI_3 :		SPI_Cmd(SPI3, DISABLE);		RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI3, DISABLE); break;
					
	}	 
#else
	switch(spi)
	{								/*		失能spi  															关闭spi时钟							*/	
		  case SPI_1 :   SPI_Cmd(SPI1, DISABLE);	 RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, DISABLE); break;
//			case SPI_2 :   SPI_Cmd(SPI2, DISABLE);	 RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI2, DISABLE); break;
//			case SPI_3 :   SPI_Cmd(SPI3, DISABLE);	 RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI3, DISABLE); break;
	}	  
	return true;
#endif
}






uint8_t SPI_ReadWriteByte(uint8_t TxData)
{		
	unsigned char retry=0;				 	
	while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET) //检查指定的SPI标志位设置与否:发送缓存空标志位
		{
		retry++;
		if(retry>200)return 0;
		}			  
	SPI_SendData8(SPI1, TxData); //通过外设SPIx发送一个数据
	retry=0;

	while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET) //检查指定的SPI标志位设置与否:接受缓存非空标志位
		{
		retry++;
		if(retry>200)return 0;
		}	  						    
	return SPI_ReceiveData8(SPI1); //返回通过SPIx最近接收的数据					    
}	








//
// typedef struct TSpi_Def
//{
//	void (*Init)(struct TSpi_Def* this);
//	void (*Uninit)(struct TSpi_Def* this);
//  bool (*WriteRead)(const SPI_CONFIGURATION& Configuration, byte* Write8, int WriteCount, byte* Read8, int ReadCount, int ReadStartOffset);
//  bool (*WriteRead16)(const SPI_CONFIGURATION& Configuration, ushort* Write16, int WriteCount, ushort* Read16, int ReadCount, int ReadStartOffset);
//
//} TSpi;

//extern void TSpi_Init(TSpi* this);
//



void TSpi_Init(TSpi* this)
{
			this->Open  			= Spi_config;
			this->Close 			= Spi_Disable;
	
//    this->WriteRead 	= 
//    this->WriteRead16	= 
}










