#include "SHT30.h"
#include "Time.h"

#define LITTLE_ENDIAN

//-- Enumerations -------------------------------------------------------------
// Sensor Commands
typedef enum{
  CMD_READ_SERIALNBR  = 0x3780, // read serial number
  CMD_READ_STATUS     = 0xF32D, // read status register
  CMD_CLEAR_STATUS    = 0x3041, // clear status register
  CMD_HEATER_ENABLE   = 0x306D, // enabled heater
  CMD_HEATER_DISABLE  = 0x3066, // disable heater
  CMD_SOFT_RESET      = 0x30A2, // soft reset
  CMD_MEAS_CLOCKSTR_H = 0x2C06, // measurement: clock stretching, high repeatability
  CMD_MEAS_CLOCKSTR_M = 0x2C0D, // measurement: clock stretching, medium repeatability
  CMD_MEAS_CLOCKSTR_L = 0x2C10, // measurement: clock stretching, low repeatability
  CMD_MEAS_POLLING_H  = 0x2400, // measurement: polling, high repeatability
  CMD_MEAS_POLLING_M  = 0x240B, // measurement: polling, medium repeatability
  CMD_MEAS_POLLING_L  = 0x2416, // measurement: polling, low repeatability
  CMD_MEAS_PERI_05_H  = 0x2032, // measurement: periodic 0.5 mps, high repeatability
  CMD_MEAS_PERI_05_M  = 0x2024, // measurement: periodic 0.5 mps, medium repeatability
  CMD_MEAS_PERI_05_L  = 0x202F, // measurement: periodic 0.5 mps, low repeatability
  CMD_MEAS_PERI_1_H   = 0x2130, // measurement: periodic 1 mps, high repeatability
  CMD_MEAS_PERI_1_M   = 0x2126, // measurement: periodic 1 mps, medium repeatability
  CMD_MEAS_PERI_1_L   = 0x212D, // measurement: periodic 1 mps, low repeatability
  CMD_MEAS_PERI_2_H   = 0x2236, // measurement: periodic 2 mps, high repeatability
  CMD_MEAS_PERI_2_M   = 0x2220, // measurement: periodic 2 mps, medium repeatability
  CMD_MEAS_PERI_2_L   = 0x222B, // measurement: periodic 2 mps, low repeatability
  CMD_MEAS_PERI_4_H   = 0x2334, // measurement: periodic 4 mps, high repeatability
  CMD_MEAS_PERI_4_M   = 0x2322, // measurement: periodic 4 mps, medium repeatability
  CMD_MEAS_PERI_4_L   = 0x2329, // measurement: periodic 4 mps, low repeatability
  CMD_MEAS_PERI_10_H  = 0x2737, // measurement: periodic 10 mps, high repeatability
  CMD_MEAS_PERI_10_M  = 0x2721, // measurement: periodic 10 mps, medium repeatability
  CMD_MEAS_PERI_10_L  = 0x272A, // measurement: periodic 10 mps, low repeatability
  CMD_FETCH_DATA      = 0xE000, // readout measurements for periodic mode
  CMD_R_AL_LIM_LS     = 0xE102, // read alert limits, low set
  CMD_R_AL_LIM_LC     = 0xE109, // read alert limits, low clear
  CMD_R_AL_LIM_HS     = 0xE11F, // read alert limits, high set
  CMD_R_AL_LIM_HC     = 0xE114, // read alert limits, high clear
  CMD_W_AL_LIM_HS     = 0x611D, // write alert limits, high set
  CMD_W_AL_LIM_HC     = 0x6116, // write alert limits, high clear
  CMD_W_AL_LIM_LC     = 0x610B, // write alert limits, low clear
  CMD_W_AL_LIM_LS     = 0x6100, // write alert limits, low set
  CMD_NO_SLEEP        = 0x303E,
}etCommands;

// Measurement Repeat
typedef enum{
  REPEATAB_HIGH,
  REPEATAB_MEDIUM,
  REPEATAB_LOW,
}etRepeatability;

// Measurement Mode
typedef enum{
  MODE_CLKSTRETCH, // clock stretching
  MODE_POLLING,    // polling
}etMode;

typedef enum{
  FREQUENCY_HZ5,  //  0.5 measurements per seconds
  FREQUENCY_1HZ,  //  1.0 measurements per seconds
  FREQUENCY_2HZ,  //  2.0 measurements per seconds
  FREQUENCY_4HZ,  //  4.0 measurements per seconds
  FREQUENCY_10HZ, // 10.0 measurements per seconds
}etFrequency;

//-- Typedefs -----------------------------------------------------------------
// Status-Register
typedef union {
  ushort u16;
  struct{
    #ifdef LITTLE_ENDIAN  // bit-order is little endian
    ushort CrcStatus     : 1; // write data checksum status
    ushort CmdStatus     : 1; // command status
    ushort Reserve0      : 2; // reserved
    ushort ResetDetected : 1; // system reset detected
    ushort Reserve1      : 5; // reserved
    ushort T_Alert       : 1; // temperature tracking alert
    ushort RH_Alert      : 1; // humidity tracking alert
    ushort Reserve2      : 1; // reserved
    ushort HeaterStatus  : 1; // heater status
    ushort Reserve3      : 1; // reserved
    ushort AlertPending  : 1; // alert pending status
    #else                 // bit-order is big endian
    ushort AlertPending  : 1;
    ushort Reserve3      : 1;
    ushort HeaterStatus  : 1;
    ushort Reserve2      : 1;
    ushort RH_Alert      : 1;
    ushort T_Alert       : 1;
    ushort Reserve1      : 5;
    ushort ResetDetected : 1;
    ushort Reserve0      : 2;
    ushort CmdStatus     : 1;
    ushort CrcStatus     : 1;
    #endif
  }bit;
} regStatus;

SHT30::SHT30()
{
	IIC		= nullptr;
	Address	= 0x44;	// ADDR=VSS
	//Address	= 0x45;	// ADDR=VDD

	Mode	= 2;
	Freq	= 5;
	Repeat	= 2;
}

SHT30::~SHT30()
{
	Pwr	= false;

	delete IIC;
	IIC = nullptr;
}

void SHT30::Init()
{
#if DEBUG
	debug_printf("\r\nSHT30::Init Addr=0x%02X ", Address);
	switch (Mode)
	{
	case 0:
		debug_printf(" 阻塞模式");	// Stretch
		break;
	case 1:
		debug_printf(" 非阻塞模式");	// Polling
		break;
	case 2:
		debug_printf(" 内部定期采集 每秒%d次", Freq);
		break;
	}
	
	debug_printf(" 重复性 ");
	switch (Repeat)
	{
	case REPEATAB_LOW:
		debug_printf("低");
		break;
	case REPEATAB_MEDIUM:
		debug_printf("中");
		break;
	case REPEATAB_HIGH:
		debug_printf("高");
		break;
	}
	debug_printf("\r\n");
#endif

	IIC->SubWidth	= 2;
	IIC->Address	= Address << 1;
	IIC->Retry		= 2;

	Pwr	= true;

	bool rs	= CheckStatus();

	/*
	SHT30三种采集数据方式：
	1，Stretch阻塞模式，发送命令后采集，需要长时间等待SCL拉高，才能发送读取头然后读取数据
	2，Polling非阻塞模式，发送命令后采集，需要反复多次启动并发送读取头，得到ACK以后才能读取数据
	3，内部定期采集模式，启动时发送Periodic命令，读取时发送FetchData命令后直接读取数据
	*/
	//Read4(CMD_MEAS_CLOCKSTR_H);
	//Read4(CMD_MEAS_POLLING_H);
	//Write(CMD_MEAS_PERI_1_H);	// 高精度重复读取，每秒一次
	if(rs) SetMode();
}

uint SHT30::ReadSerialNumber() const
{
	return Read4(CMD_READ_SERIALNBR);
}

ushort SHT30::ReadStatus() const
{
	return Read2(CMD_READ_STATUS);
}

bool SHT30::Read(ushort& temp, ushort& humi)
{
	/*
	SHT30三种采集数据方式：
	1，Stretch阻塞模式，发送命令后采集，需要长时间等待SCL拉高，才能发送读取头然后读取数据
	2，Polling非阻塞模式，发送命令后采集，需要反复多次启动并发送读取头，得到ACK以后才能读取数据
	3，内部定期采集模式，启动时发送Periodic命令，读取时发送FetchData命令后直接读取数据
	*/
	//TimeCost tc;
	//uint data = Read4(CMD_MEAS_CLOCKSTR_H);
	//uint data = Read4(CMD_MEAS_POLLING_H);
	//uint data = Read4(CMD_FETCH_DATA);
	uint data = Read4(GetMode());
	//tc.Show();
	if(!data)
	{
		bool rs	= CheckStatus();
		if(rs) SetMode();

		return false;
	}

	temp = data >> 16;
	// 公式:T= -46.85 + 175.72 * ST/2^16
	temp = ((temp * 17572) >> 16) - 4685;
	//temp /= 100;

	humi = data & 0xFFFF;
	// 公式: RH%= -6 + 125 * SRH/2^16
	humi = ((humi * 125 * 100) >> 16) - 6 * 100;

	return true;
}

bool SHT30::Write(ushort cmd)
{
	//debug_printf("cmd=0x%04X \r\n", cmd);

	// 只有子操作码，没有数据
	bool rs = IIC->Write(cmd, ByteArray(0));
	if(!rs) debug_printf("SHT30::Write 0x%04X 失败\r\n", cmd);

	return rs;
}

ushort SHT30::Read2(ushort cmd) const
{
	ByteArray rs(3);
	if(IIC->Read(cmd, rs) == 0)
	{
		debug_printf("SHT30::Read2 0x%04X 失败\r\n", cmd);
		return 0;
	}

	return (rs[0] << 8) | rs[1];
}

// 同时读取温湿度并校验Crc
uint SHT30::Read4(ushort cmd) const
{
	if(!cmd) return 0;

	ByteArray rs(6);
	if(IIC->Read(cmd, rs) == 0)
	{
		debug_printf("SHT30::Read4 0x%04X 失败\r\n", cmd);
		return 0;
	}

	// 分解数据，暂时不进行CRC校验
	byte* p = rs.GetBuffer();
	ushort temp = _REV16(*(ushort*)p);
	ushort humi = _REV16(*(ushort*)(p + 3));

	return (temp << 16) | humi;
}

bool SHT30::CheckStatus()
{
	ushort st	= ReadStatus();
	regStatus rs;
	rs.u16	= st;
#if DEBUG
	if(st != 0)
	{
		uint sn		= ReadSerialNumber();
		debug_printf("SNum=0x%08X Status=0x%04X \r\n", sn, st);
		debug_printf("Status=0x%04X \r\n", st);
		//if(rs.bit.ResetDetected)		debug_printf(" RstDet");	// ResetDetected
		//if(rs.bit.HeaterStatus)		debug_printf(" HatSta");	// HeaterStatus
		//if(rs.bit.AlertPending)		debug_printf(" AltPed");	// AlertPending
		//debug_printf("\r\n");
	}
#endif

	if(!rs.bit.ResetDetected) return false;

	Write(CMD_CLEAR_STATUS);

	return true;
}

void SHT30::SetMode()
{
	if(Mode != 2) return;

	debug_printf("SHT30::SetMode Repeat=%d Freq=%d \r\n", Repeat, Freq);
	//Write(CMD_MEAS_PERI_1_H);	// 高精度重复读取，每秒一次
	static ushort cmds[]	= {
		CMD_MEAS_PERI_05_H, CMD_MEAS_PERI_1_H, CMD_MEAS_PERI_2_H, CMD_MEAS_PERI_4_H, CMD_MEAS_PERI_10_H,
		CMD_MEAS_PERI_05_M, CMD_MEAS_PERI_1_M, CMD_MEAS_PERI_2_M, CMD_MEAS_PERI_4_M, CMD_MEAS_PERI_10_M,
		CMD_MEAS_PERI_05_L, CMD_MEAS_PERI_1_L, CMD_MEAS_PERI_2_L, CMD_MEAS_PERI_4_L, CMD_MEAS_PERI_10_L,
	};
	static byte maps[]	= { 0, 1, 2, 0, 3, 0, 0, 0, 0, 0, 4 };
	// 5/1/2/4/10 转为 0/1/2/3/4
	byte map	= maps[Freq];
	ushort cmd	= cmds[map + Repeat * 5];
	bool rs		= Write(cmd);

	// 软重启
	if(!rs) rs	= Write(CMD_SOFT_RESET);
	// 硬重启
	if(!rs) Pwr.Down(100);
}

ushort SHT30::GetMode() const
{
	static ushort cs[] = {CMD_MEAS_CLOCKSTR_L, CMD_MEAS_CLOCKSTR_M, CMD_MEAS_CLOCKSTR_H};
	static ushort ps[] = {CMD_MEAS_POLLING_L, CMD_MEAS_POLLING_M, CMD_MEAS_POLLING_H};
	switch(Mode)
	{
		case 0:
		{
			if(Repeat < ArrayLength(cs)) return cs[Repeat];
			break;
		}
		case 1:
		{
			if(Repeat < ArrayLength(ps)) return ps[Repeat];
			break;
		}
		case 2:
		{
			return CMD_FETCH_DATA;
		}
	}

	return 0;
}

void SHT30::ChangePower(int level)
{
	if(!level) Write(CMD_NO_SLEEP);
	// 进入低功耗时，直接关闭
	//if(level) Close();
}
