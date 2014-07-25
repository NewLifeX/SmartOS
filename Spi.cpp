#include "Sys.h"
#include "Port.h"
#include "Spi.h"

#ifdef STM32F10X
	#include "stm32f10x_spi.h"
#else
	#include "stm32f0xx_spi.h"
#endif

#include "Pin.h"

static const  Pin  g_Spi_Pins_Map[][4]=  SPI_PINS_FULLREMAP;

Spi::Spi(int spi, int speedMHz, bool useNss)
{
	const Pin* ps = g_Spi_Pins_Map[spi];		//选定spi引脚

	if(spi > 3) return;
    
    // 计算速度
    ushort pre = 0;
    int n = Sys.Clock / speedMHz;   // 分频
    switch(n)
    {
        case 2: pre = SPI_BaudRatePrescaler_2; break;
        case 4: pre = SPI_BaudRatePrescaler_4; break;
        case 8: pre = SPI_BaudRatePrescaler_8; break;
        case 16: pre = SPI_BaudRatePrescaler_16; break;
        case 32: pre = SPI_BaudRatePrescaler_32; break;
        case 64: pre = SPI_BaudRatePrescaler_64; break;
        case 128: pre = SPI_BaudRatePrescaler_128; break;
        case 256: pre = SPI_BaudRatePrescaler_256; break;
        default:
            printf("Spi%d Init Error! speedMHz=%d mush be dived with %d\r\n", spi, speedMHz, Sys.Clock);
            return;
    }

    Speed = speedMHz;

    //_nss = useNss ? spi_nss[spi] : P0;
    _nss = P0;
    if(useNss)
    {
        _nss = ps[0];
        if(spi == SPI_3)
        {
            //PA15是jtag接口中的一员 想要使用 必须开启remap
            RCC_APB2PeriphClockCmd( RCC_APB2Periph_AFIO, ENABLE);	
            GPIO_PinRemapConfig( GPIO_Remap_SWJ_JTAGDisable, ENABLE);
        }
        Port::SetOutput(_nss, false);
    }
    
#ifdef STM32F10X
	Port::SetAlternate(ps[1], false);
	Port::SetAlternate(ps[2], false);
	Port::SetAlternate(ps[3], false);

    /*使能SPI1时钟*/
	switch(spi)
	{
		case SPI_1 :RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE); break;
		case SPI_2 :RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE); break;
		case SPI_3 :RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3, ENABLE); break;
	}	  
    /* 拉高csn引脚，释放spi总线*/
	Stop();

	SPI_InitTypeDef sp;
	sp.SPI_Direction = SPI_Direction_2Lines_FullDuplex; //双线全双工
	sp.SPI_Mode = SPI_Mode_Master;	 					//主模式
	sp.SPI_DataSize = SPI_DataSize_8b;	 				//数据大小8位
	sp.SPI_CPOL = SPI_CPOL_Low;                         //时钟极性，空闲时为低
	sp.SPI_CPHA = SPI_CPHA_1Edge;						//第1个边沿有效，上升沿为采样时刻
	sp.SPI_NSS = SPI_NSS_Soft;		   					//NSS信号由软件产生
	sp.SPI_BaudRatePrescaler = pre;                     //8分频，9MHz
	sp.SPI_FirstBit = SPI_FirstBit_MSB;  				//高位在前
	sp.SPI_CRCPolynomial = 7;

	switch(spi)
	{
		case SPI_1 : SPI_Init(SPI1, &sp);  SPI_Cmd(SPI1, ENABLE);	break;
		case SPI_2 : SPI_Init(SPI2, &sp);  SPI_Cmd(SPI2, ENABLE);	break;
		case SPI_3 : SPI_Init(SPI3, &sp);  SPI_Cmd(SPI3, ENABLE);	break;	
	}	  
#else
	Port::SetAlternate(ps[1], false, Port::Speed_10MHz);
	Port::SetAlternate(ps[2], false, Port::Speed_10MHz);
	Port::SetAlternate(ps[3], false, Port::Speed_10MHz);
	// SPI都在GPIO_AF_0分组内
    GPIO_PinAFConfig(_GROUP(ps[1]), _PIN(ps[1]), GPIO_AF_0);
    GPIO_PinAFConfig(_GROUP(ps[2]), _PIN(ps[2]), GPIO_AF_0);
    GPIO_PinAFConfig(_GROUP(ps[3]), _PIN(ps[3]), GPIO_AF_0);

	switch(spi)
	{								
        case SPI_1 : RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);  	break;
	}	  
	Stop();

	sp.SPI_Direction = SPI_Direction_2Lines_FullDuplex;  //设置SPI单向或者双向的数据模式:SPI设置为双线双向全双工
	sp.SPI_Mode = SPI_Mode_Master;		//设置SPI工作模式:设置为主SPI
	sp.SPI_DataSize = SPI_DataSize_8b;		//设置SPI的数据大小:SPI发送接收8位帧结构
	sp.SPI_CPOL = SPI_CPOL_Low;		//选择了串行时钟的稳态:时钟悬空高
	sp.SPI_CPHA = SPI_CPHA_1Edge;	//数据捕获于第二个时钟沿
	sp.SPI_NSS = SPI_NSS_Soft;		//NSS信号由硬件（NSS管脚）还是软件（使用SSI位）管理:内部NSS信号有SSI位控制
	sp.SPI_BaudRatePrescaler = pre;		//定义波特率预分频的值:波特率预分频值为
	sp.SPI_FirstBit = SPI_FirstBit_MSB;	//指定数据传输从MSB位还是LSB位开始:数据传输从MSB位开始
	sp.SPI_CRCPolynomial = 7;	//CRC值计算的多项式

	switch(spi)
	{
        case SPI_1 : SPI_Init(SPI1, &sp);  SPI_Cmd(SPI1, ENABLE);	break;
	}
#endif
    
    _spi = spi;
}
	
Spi::~Spi()
{
    Stop();

#ifdef STM32F10X
	switch(_spi)
	{
        /* 失能spi 关闭spi时钟 */	
        case SPI_1 :		SPI_Cmd(SPI1, DISABLE);		RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, DISABLE); break;
        case SPI_2 :		SPI_Cmd(SPI2, DISABLE);		RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, DISABLE); break;
        case SPI_3 :		SPI_Cmd(SPI3, DISABLE);		RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3, DISABLE); break;
	}	 
#else
	switch(_spi)
	{
        /* 失能spi 关闭spi时钟 */	
        case SPI_1 :   SPI_Cmd(SPI1, DISABLE);	 RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, DISABLE); break;
        case SPI_2 :   SPI_Cmd(SPI2, DISABLE);	 RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI2, DISABLE); break;
        case SPI_3 :   SPI_Cmd(SPI3, DISABLE);	 RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI3, DISABLE); break;
	}	  
#endif
}

byte Spi::ReadWriteByte8(byte data)
{		
	unsigned char retry=0;				 
	SPI_TypeDef *	p;
    switch(_spi)
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
	SPI_I2S_SendData(p, data);
#else
	SPI_SendData8(p, data);
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

ushort Spi::ReadWriteByte16(ushort data)
{
 	uint retry=0;				 
	SPI_TypeDef* p;
	switch(_spi)
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
	SPI_I2S_SendData(p, data);
#else
	SPI_I2S_SendData16(p, data);
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

// 拉低NSS，开始传输
void Spi::Start()
{
    if(_nss != P0) Port::Write(_nss, false);
}

// 拉高NSS，停止传输
void Spi::Stop()
{
    if(_nss != P0) Port::Write(_nss, true);
}
