#include "Buffer.h"
#include "Sys.h"

#include "Spi.h"

#include "Platform\stm32.h"

int Spi::GetPre(int index, uint& speedHz)
{
	// 自动计算稍低于速度speedHz的分频
	ushort pre = SPI_BaudRatePrescaler_2;
	uint clock = Sys.Clock >> 1;
	while(pre <= SPI_BaudRatePrescaler_256)
	{
		if(clock <= speedHz) break;
		clock >>= 1;
		pre += (SPI_BaudRatePrescaler_4 - SPI_BaudRatePrescaler_2);
	}
	if(pre > SPI_BaudRatePrescaler_256)
	{
		debug_printf("Spi%d::Init Error! speedHz=%d mush be dived with %d\r\n", index, speedHz, Sys.Clock);
		return -1;
	}

	speedHz = clock;
	
	return pre;
}

void Spi::OnInit()
{
	SPI_TypeDef* g_Spis[] = SPIS;
	assert_param(_index < ArrayLength(g_Spis));

    _SPI = g_Spis[_index];

	Pin g_Spi_Pins_Map[][4] =  SPI_PINS_FULLREMAP;
	auto ps = g_Spi_Pins_Map[_index];		//选定spi引脚
	Buffer(Pins, sizeof(Pins))	= ps;
}

void Spi::OnOpen()
{
	// 自动计算稍低于速度speedHz的分频
	uint speedHz = Speed;
	int pre = GetPre(_index, speedHz);
	if(pre == -1) return;

    // 使能SPI时钟
	switch(_index)
	{
		case 0: RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE); break;
#if defined(STM32F1) || defined(STM32F4)
		case 1: RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE); break;
		case 2: RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3, ENABLE); break;
#if defined(STM32F4)
		case 3: RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI4, ENABLE); break;
		case 4: RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI5, ENABLE); break;
		case 5: RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI6, ENABLE); break;
#endif
#endif
	}

#if defined(STM32F0)
	// SPI都在GPIO_AF_0分组内
	_clk.AFConfig(Port::AF_0);
	_miso.AFConfig(Port::AF_0);
	_mosi.AFConfig(Port::AF_0);
#elif defined(STM32F4)
	const byte afs[] = { GPIO_AF_SPI1, GPIO_AF_SPI2, GPIO_AF_SPI3, GPIO_AF_SPI4, GPIO_AF_SPI5, GPIO_AF_SPI6 };
	_clk.AFConfig((Port::GPIO_AF)afs[_index]);
	_miso.AFConfig((Port::GPIO_AF)afs[_index]);
	_mosi.AFConfig((Port::GPIO_AF)afs[_index]);
#endif

	Stop();
	SPI_I2S_DeInit((SPI_TypeDef*)_SPI);
	//SPI_DeInit((SPI_TypeDef*)_SPI);	// SPI_I2S_DeInit的宏定义别名

	SPI_InitTypeDef sp;
    SPI_StructInit(&sp);
	sp.SPI_Direction = SPI_Direction_2Lines_FullDuplex; //双线全双工
	sp.SPI_Mode = SPI_Mode_Master;      // 主模式
	sp.SPI_DataSize = SPI_DataSize_8b;  // 数据大小8位 SPI发送接收8位帧结构
	sp.SPI_CPOL = SPI_CPOL_Low;         // 时钟极性，空闲时为低
	sp.SPI_CPHA = SPI_CPHA_1Edge;       // 第1个边沿有效，上升沿为采样时刻
	sp.SPI_NSS = SPI_NSS_Soft;          // NSS信号由硬件（NSS管脚）还是软件（使用SSI位）管理:内部NSS信号有SSI位控制
	sp.SPI_BaudRatePrescaler = pre;     // 8分频，9MHz 定义波特率预分频的值
	sp.SPI_FirstBit = SPI_FirstBit_MSB; // 高位在前。指定数据传输从MSB位还是LSB位开始:数据传输从MSB位开始
	sp.SPI_CRCPolynomial = 7;           // CRC值计算的多项式

    SPI_Init((SPI_TypeDef*)_SPI, &sp);
    SPI_Cmd((SPI_TypeDef*)_SPI, ENABLE);
}

void Spi::OnClose()
{
	SPI_Cmd((SPI_TypeDef*)_SPI, DISABLE);
	SPI_I2S_DeInit((SPI_TypeDef*)_SPI);
}

byte Spi::Write(byte data)
{
	if(!Opened) Open();

	auto si	= (SPI_TypeDef*)_SPI;
	int retry = Retry;
    while (SPI_I2S_GetFlagStatus(si, SPI_I2S_FLAG_TXE) == RESET)
    {
        if(--retry <= 0) return ++Error; // 超时处理
    }

#ifndef STM32F0
	SPI_I2S_SendData(si, data);
#else
	SPI_SendData8(si, data);
#endif

	retry = Retry;
	while (SPI_I2S_GetFlagStatus(si, SPI_I2S_FLAG_RXNE) == RESET) //是否发送成功
    {
        if(--retry <= 0) return ++Error; // 超时处理
    }
#ifndef STM32F0
	return SPI_I2S_ReceiveData(si);
#else
	return SPI_ReceiveData8(si); //返回通过SPIx最近接收的数据
#endif
}

ushort Spi::Write16(ushort data)
{
	if(!Opened) Open();

	auto si	= (SPI_TypeDef*)_SPI;
    // 双字节操作，超时次数加倍
	int retry = Retry << 1;
	while (SPI_I2S_GetFlagStatus(si, SPI_I2S_FLAG_TXE) == RESET)
	{
        if(--retry <= 0) return ++Error; // 超时处理
	}

#ifndef STM32F0
	SPI_I2S_SendData(si, data);
#else
	SPI_I2S_SendData16(si, data);
#endif

    retry = Retry << 1;
	while (SPI_I2S_GetFlagStatus(si, SPI_I2S_FLAG_RXNE) == RESET)
	{
        if(--retry <= 0) return ++Error; // 超时处理
	}

#ifndef STM32F0
	return SPI_I2S_ReceiveData(si);
#else
	return SPI_I2S_ReceiveData16(si);
#endif
}
