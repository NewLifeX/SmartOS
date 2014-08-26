#include "Sys.h"
#include "Port.h"
#include "Spi.h"

static SPI_TypeDef* g_Spis[] = SPIS;
static const Pin g_Spi_Pins_Map[][4] =  SPI_PINS_FULLREMAP;

int GetPre(int spi, uint* speedHz)
{
	// 自动计算稍低于速度speedHz的分频
	ushort pre = SPI_BaudRatePrescaler_2;
	uint clock = Sys.Clock >> 1;
	while(pre <= SPI_BaudRatePrescaler_256)
	{
		if(clock <= *speedHz) break;
		clock >>= 1;
		pre += (SPI_BaudRatePrescaler_4 - SPI_BaudRatePrescaler_2);
	}
	if(pre > SPI_BaudRatePrescaler_256)
	{
		debug_printf("Spi%d Init Error! speedHz=%d mush be dived with %d\r\n", spi, *speedHz, Sys.Clock);
		return -1;
	}

	*speedHz = clock;
	return pre;
}

Spi::Spi(int spi, uint speedHz, bool useNss)
{
	assert_param(spi >= 0 && spi < ArrayLength(g_Spis));

    _spi = spi;
	Opened = false;
    SPI = g_Spis[spi];
	const Pin* ps = g_Spi_Pins_Map[spi];		//选定spi引脚
	memcpy(Pins, ps, sizeof(Pins));

	if(!useNss) Pins[0] = P0;

	// 自动计算稍低于速度speedHz的分频
	int pre = GetPre(spi, &speedHz);
	if(pre == -1) return;

    debug_printf("Spi%d %dHz Nss:%d\r\n", spi + 1, speedHz, useNss);

    Speed = speedHz;
    Retry = 200;
}

Spi::~Spi()
{
    debug_printf("~Spi%d\r\n", _spi + 1);

	Close();
}

void Spi::SetPin(Pin clk, Pin miso, Pin mosi, Pin nss)
{
	if(nss != P0) Pins[0] = nss;
	if(clk != P0) Pins[1] = clk;
	if(miso != P0) Pins[2] = miso;
	if(mosi != P0) Pins[3] = mosi;
}

void Spi::GetPin(Pin* clk, Pin* miso, Pin* mosi, Pin* nss)
{
	//const Pin* ps = g_Spi_Pins_Map[_spi];
	if(nss) *nss = Pins[0];
	if(clk) *clk = Pins[1];
	if(miso) *miso = Pins[2];
	if(mosi) *mosi = Pins[3];
}

void Spi::Open()
{
	if(Opened) return;

	// 自动计算稍低于速度speedHz的分频
	uint speedHz = Speed;
	int pre = GetPre(_spi, &speedHz);
	if(pre == -1) return;

	Pin* ps = Pins;
    // 端口配置，销毁Spi对象时才释放
    debug_printf("    CLK : ");
    _clk = new AlternatePort(ps[1]);
    debug_printf("    MISO: ");
    _miso = new AlternatePort(ps[2]);
    debug_printf("    MOSI: ");
    _mosi = new AlternatePort(ps[3]);

    if(ps[0] != P0)
    {
#ifdef STM32F10X
        /*if(SPI == SPI3)
        {
        }*/
#endif
		debug_printf("    NSS : ");
        _nss = new OutputPort(ps[0]);
		*_nss = true; // 拉高进入空闲状态
    }

    // 使能SPI时钟
	switch(_spi)
	{
		case SPI_1 :RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE); break;
#if defined(STM32F1) || defined(STM32F4)
		case SPI_2 :RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE); break;
		case SPI_3 :RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3, ENABLE); break;
#if defined(STM32F4)
		case SPI_4 :RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI4, ENABLE); break;
		case SPI_5 :RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI5, ENABLE); break;
		case SPI_6 :RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI6, ENABLE); break;
#endif
#endif
	}

#if defined(STM32F0)
	// SPI都在GPIO_AF_0分组内
    GPIO_PinAFConfig(_GROUP(ps[1]), _PIN(ps[1]), GPIO_AF_0);
    GPIO_PinAFConfig(_GROUP(ps[2]), _PIN(ps[2]), GPIO_AF_0);
    GPIO_PinAFConfig(_GROUP(ps[3]), _PIN(ps[3]), GPIO_AF_0);
#elif defined(STM32F4)
	byte afs[] = { GPIO_AF_SPI1, GPIO_AF_SPI2, GPIO_AF_SPI3, GPIO_AF_SPI4, GPIO_AF_SPI5, GPIO_AF_SPI6,  };
    GPIO_PinAFConfig(_GROUP(ps[1]), _PIN(ps[1]), afs[_spi]);
    GPIO_PinAFConfig(_GROUP(ps[2]), _PIN(ps[2]), afs[_spi]);
    GPIO_PinAFConfig(_GROUP(ps[3]), _PIN(ps[3]), afs[_spi]);
#endif

	Stop();
#if defined(STM32F0)	
	SPI_I2S_DeInit(SPI);
#else		
	SPI_DeInit(SPI);	// SPI_I2S_DeInit的宏定义别名
#endif
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

    SPI_Init(SPI, &sp);
    SPI_Cmd(SPI, ENABLE);

	Stop();

	Opened = true;
}

void Spi::Close()
{
	if(!Opened) return;

    Stop();

	SPI_Cmd(SPI, DISABLE);
	SPI_I2S_DeInit(SPI);

	debug_printf("    CLK : ");
	if(_clk) delete _clk;
	debug_printf("    MISO: ");
	if(_miso) delete _miso;
	debug_printf("    MOSI: ");
	if(_mosi) delete _mosi;
	_clk = NULL;
	_miso = NULL;
	_mosi = NULL;

	debug_printf("    NSS : ");
	if(_nss) delete _nss;
	_nss = NULL;

	Opened = false;
}

byte Spi::Write(byte data)
{
	int retry = Retry;
    while (SPI_I2S_GetFlagStatus(SPI, SPI_I2S_FLAG_TXE) == RESET)
    {
        if(--retry <= 0) return ++Error; // 超时处理
    }

#ifndef STM32F0
	SPI_I2S_SendData(SPI, data);
#else
	SPI_SendData8(SPI, data);
#endif

	retry = Retry;
	while (SPI_I2S_GetFlagStatus(SPI, SPI_I2S_FLAG_RXNE) == RESET) //是否发送成功
    {
        if(--retry <= 0) return ++Error; // 超时处理
    }
#ifndef STM32F0
	return SPI_I2S_ReceiveData(SPI);
#else
	return SPI_ReceiveData8(SPI); //返回通过SPIx最近接收的数据
#endif
}

ushort Spi::Write16(ushort data)
{
    // 双字节操作，超时次数加倍
	int retry = Retry << 1;
	while (SPI_I2S_GetFlagStatus(SPI, SPI_I2S_FLAG_TXE) == RESET)
	{
        if(--retry <= 0) return ++Error; // 超时处理
	}

#ifndef STM32F0
	SPI_I2S_SendData(SPI, data);
#else
	SPI_I2S_SendData16(SPI, data);
#endif

    retry = Retry << 1;
	while (SPI_I2S_GetFlagStatus(SPI, SPI_I2S_FLAG_RXNE) == RESET)
	{
        if(--retry <= 0) return ++Error; // 超时处理
	}

#ifndef STM32F0
	return SPI_I2S_ReceiveData(SPI);
#else
	return SPI_I2S_ReceiveData16(SPI);
#endif
}

// 拉低NSS，开始传输
void Spi::Start()
{
    //if(_nss != P0) Port::Write(_nss, false);
    if(_nss) *_nss = false;

    // 开始新一轮事务操作，错误次数清零
    Error = 0;
}

// 拉高NSS，停止传输
void Spi::Stop()
{
    //if(_nss != P0) Port::Write(_nss, true);
    if(_nss) *_nss = true;
}
