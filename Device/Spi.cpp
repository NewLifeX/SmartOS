#include "Sys.h"

#include "Spi.h"

Spi::Spi()
{
	Init();
}

Spi::Spi(SPI spi, uint speedHz, bool useNss)
{
	Init();

	Init(spi, speedHz, useNss);
}

Spi::~Spi()
{
    debug_printf("Spi::~Spi%d\r\n", _index + 1);

	Close();
}

void Spi::Init()
{
	_index	= 0xFF;
    Retry	= 200;
	Opened	= false;
	Pins[0]	= Pins[1]	= Pins[2]	= Pins[3]	= P0;
}

void Spi::Init(SPI spi, uint speedHz, bool useNss)
{
	_index = spi;

	OnInit();

	if(!useNss) Pins[0] = P0;

#if DEBUG
	int k = speedHz/1000;
	int m = k/1000;
	k -= m * 1000;
	if(k == 0)
		debug_printf("Spi%d::Init %dMHz Nss:%d\r\n", _index + 1, m, useNss);
	else
		debug_printf("Spi%d::Init %d.%dMHz Nss:%d\r\n", _index + 1, m, k, useNss);
#endif

	// 自动计算稍低于速度speedHz的分频
	int pre = GetPre(_index, speedHz);
	if(pre == -1) return;

    Speed = speedHz;
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
	if(nss) *nss = Pins[0];
	if(clk) *clk = Pins[1];
	if(miso) *miso = Pins[2];
	if(mosi) *mosi = Pins[3];
}

void Spi::Open()
{
	if(Opened) return;

#if DEBUG
	int k = Speed/1000;
	int m = k/1000;
	k -= m * 1000;
	if(k == 0)
		debug_printf("Spi%d::Open %dMHz\r\n", _index + 1, m);
	else
		debug_printf("Spi%d::Open %d.%dMHz\r\n", _index + 1, m, k);
#endif

	// 自动计算稍低于速度speedHz的分频
	uint speedHz = Speed;
	int pre = GetPre(_index, speedHz);
	if(pre == -1) return;

	Pin* ps = Pins;
    // 端口配置，销毁Spi对象时才释放
    debug_printf("\t CLK : ");
    _clk.Set(ps[1]).Open();
    debug_printf("\t MISO: ");
    _miso.Set(ps[2]).Open();
    debug_printf("\t MOSI: ");
    _mosi.Set(ps[3]).Open();

    if(ps[0] != P0)
    {
		debug_printf("    NSS : ");
		_nss.OpenDrain = false;
		//_nss.Set(ps[0]).Open();
		_nss.Init(ps[0], true).Open();
    }

    OnOpen();

	Stop();

	Opened = true;
}

void Spi::Close()
{
	if(!Opened) return;

    Stop();

	OnClose();

	debug_printf("\t CLK : ");
	_clk.Close();
	debug_printf("\t MISO: ");
	_miso.Close();
	debug_printf("\t MOSI: ");
	_mosi.Close();
	debug_printf("\t NSS : ");
	_nss.Close();

	Opened = false;
}

// 批量读写。以字节数组长度为准
void Spi::Write(const Buffer& bs)
{
	for(int i=0; i<bs.Length(); i++)
	{
		Write(bs[i]);
	}
}

void Spi::Read(Buffer& bs)
{
	for(int i=0; i<bs.Length(); i++)
	{
		bs[i] = Write(0x00);
	}
}

// 拉低NSS，开始传输
void Spi::Start()
{
    if(!_nss.Empty()) _nss = true;

    // 开始新一轮事务操作，错误次数清零
    Error = 0;
}

// 拉高NSS，停止传输
void Spi::Stop()
{
    if(!_nss.Empty()) _nss = false;
}
