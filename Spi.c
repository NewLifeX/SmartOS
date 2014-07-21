#include "System.h"

#ifdef STM32F10X
	#include "stm32f10x_spi.h"
	#include "Pin_STM32F1.h"
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
#define IS_SPI(a)	   if(a>3){return false;}

//#define SPI_NSS_PINS  {4, 28, 15} // PA4, PB12, PA15
//#define SPI_SCLK_PINS {5, 29, 19} // PA5, PB13, PB3
//#define SPI_MISO_PINS {6, 30, 20} // PA6, PB14, PB4
//#define SPI_MOSI_PINS {7, 31, 21} // PA7, PB15, PB5

#ifdef STM32F10X
void gpio_config(const Pin  pin[])
{

}
#else
void gpio_config(const Pin pin[])
{
    Sys.IO.OpenPort(pin[1], GPIO_Mode_AF, GPIO_Speed_10MHz, GPIO_OType_PP,GPIO_PuPd_NOPULL);
    Sys.IO.OpenPort(pin[2], GPIO_Mode_AF, GPIO_Speed_10MHz, GPIO_OType_PP,GPIO_PuPd_NOPULL);
    Sys.IO.OpenPort(pin[3], GPIO_Mode_AF, GPIO_Speed_10MHz, GPIO_OType_PP,GPIO_PuPd_NOPULL);
		//spi都在GPIO_AF_0分组内
    GPIO_PinAFConfig(_GROUP(pin[1]), _PIN(pin[1]), GPIO_AF_0);
    GPIO_PinAFConfig(_GROUP(pin[2]), _PIN(pin[2]), GPIO_AF_0);
    GPIO_PinAFConfig(_GROUP(pin[3]), _PIN(pin[3]), GPIO_AF_0);
}
#endif
bool Spi_config(int spi)
{
	
	const Pin  * p= g_Spi_Pins_Map[spi];
#ifdef STM32F1XX
	
#else
	SPI_InitTypeDef SPI_InitStructure;
#endif	
	IS_SPI(spi)
	gpio_config(p);
#ifdef STM32F10X
  /*使能SPI1时钟*/
	switch(spi)
	{
		case SPI_1 :RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE); break;
		case SPI_2 :RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE); break;
		case SPI_3 :RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3, ENABLE); break;
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
	printf("   初始化SPI_%d\r\n",(spi+1));
	switch(spi)
	{								/*				配置spi													使能spi  */	
		case SPI_1 : SPI_Init(SPI1, &SPI_InitStructure);  SPI_Cmd(SPI1, ENABLE);	break;
		case SPI_2 : SPI_Init(SPI2, &SPI_InitStructure);  SPI_Cmd(SPI2, ENABLE);	break;
		case SPI_3 : SPI_Init(SPI3, &SPI_InitStructure);  SPI_Cmd(SPI3, ENABLE);	break;	
	}	  
#else
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
	printf("   初始化SPI_%d\r\n",(spi+1));
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
	IS_SPI(spi)
	SPI_CS_HIGH(spi);		//使从机释放总线
#ifdef STM32F10X
	switch(spi)
	{								/*		失能spi  															关闭spi时钟							*/	
		case SPI_1 :		SPI_Cmd(SPI1, DISABLE);		RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, DISABLE); break;
		case SPI_2 :		SPI_Cmd(SPI2, DISABLE);		RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, DISABLE); break;
		case SPI_3 :		SPI_Cmd(SPI3, DISABLE);		RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3, DISABLE); break;
	}	 
//	printf("   关闭SPI_%d\r\n",(spi+1));
	return true;
#else
	switch(spi)
	{								/*		失能spi  															关闭spi时钟							*/	
		  case SPI_1 :   SPI_Cmd(SPI1, DISABLE);	 RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, DISABLE); break;
//			case SPI_2 :   SPI_Cmd(SPI2, DISABLE);	 RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI2, DISABLE); break;
//			case SPI_3 :   SPI_Cmd(SPI3, DISABLE);	 RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI3, DISABLE); break;
	}	  
//		printf("   关闭SPI_%d\r\n",(spi+1));
	return true;
#endif
}

byte SPI_ReadWriteByte8(int spi,byte TxData)
{		
	unsigned char retry=0;				 
	SPI_TypeDef *	p;
		switch(spi)
	{							
		case SPI_1 : p=  SPI1 ; break;

#ifdef STM32F10X
		case SPI_2 : p=  SPI2;  break;
		case SPI_3 : p=  SPI3; 	break;
#endif
	}	 
	while (SPI_I2S_GetFlagStatus(p, SPI_I2S_FLAG_TXE) == RESET) 
		{
		retry++;
		if(retry>200)return 0;		//超时处理
		}		
 
#ifdef STM32F10X
	SPI_I2S_SendData(p, TxData);
#else
	SPI_I2S_SendData8(p, HalfWord);
#endif		
	retry=0;
	while (SPI_I2S_GetFlagStatus(p, SPI_I2S_FLAG_RXNE) == RESET) //是否发送成功
		{
		retry++;
		if(retry>200)return 0;		//超时处理
		}	 
#ifdef STM32F10X
	return SPI_I2S_ReceiveData(p);
#else 						    
	return SPI_ReceiveData8(p); //返回通过SPIx最近接收的数据			
#endif		
}	

ushort SPI_ReadWriteByte16(int spi,ushort HalfWord)
{
 	uint retry=0;				 
	SPI_TypeDef *	p;
	switch(spi)
	{							
		case SPI_1 : p=  SPI1 ; break;

#ifdef STM32F10X
		case SPI_2 : p=  SPI2;  break;
		case SPI_3 : p=  SPI3; 	break;
#endif
	}	 
	while (SPI_I2S_GetFlagStatus(p, SPI_I2S_FLAG_TXE) == RESET)
	{
		retry++;
		if(retry>500)return 0;		//超时处理
	}			
 
#ifdef STM32F10X
	SPI_I2S_SendData(p, HalfWord);
#else
	SPI_I2S_SendData16(p, HalfWord);
#endif
	while (SPI_I2S_GetFlagStatus(p, SPI_I2S_FLAG_RXNE) == RESET)
	{
		retry++;
		if(retry>500)return 0;		//超时处理
	}			
	
#ifdef STM32F10X
	return SPI_I2S_ReceiveData(p);
#else
	return SPI_I2S_ReceiveData16(p);
#endif
}

void TSpi_Init(TSpi* this)
{
			this->Open  					= Spi_config;
			this->Close 					= Spi_Disable;
			this->WriteReadByte8 	= SPI_ReadWriteByte8;
			this->WriteReadByte16	=	SPI_ReadWriteByte16;
//    this->WriteRead 	= 
//    this->WriteRead16	= 
}

