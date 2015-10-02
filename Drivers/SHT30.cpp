#include "SHT30.h"

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

// Measurement Repeatability
typedef enum{
  REPEATAB_HIGH,   // high repeatability
  REPEATAB_MEDIUM, // medium repeatability
  REPEATAB_LOW,    // low repeatability
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
	IIC		= NULL;
	Address	= 0x44;	// ADDR=VSS
	//Address	= 0x45;	// ADDR=VDD
}

SHT30::~SHT30()
{
	delete IIC;
	IIC = NULL;
}

void SHT30::Init()
{
	IIC->SubWidth	= 2;
	IIC->Address	= Address << 1;

	Write(CMD_SOFT_RESET);	// 软重启
	Sys.Sleep(15);

	uint sn = ReadSerialNumber();
	ushort st = ReadStatus();
	//regStatus pst;
	//pst.u16 = st;
	debug_printf("SHT30::Init SerialNumber=0x%08X Status=0x%04X \r\n", sn, st);
}

uint SHT30::ReadSerialNumber()
{
	return Read4(CMD_READ_SERIALNBR);
}

ushort SHT30::ReadStatus()
{
	return Read2(CMD_READ_STATUS);
}

ushort SHT30::ReadTemperature()
{
	if(!IIC) return 0;

	ushort n = Read4(CMD_MEAS_CLOCKSTR_H) >> 16;
	// 公式:T= -46.85 + 175.72 * ST/2^16
	/*n = n * 17572 / 65535 - 4685;
	n /= 100;*/

	return n;
}

ushort SHT30::ReadHumidity()
{
	if(!IIC) return 0;

	ushort n = Read4(CMD_MEAS_CLOCKSTR_H) & 0xFFFF;
	// 公式: RH%= -6 + 125 * SRH/2^16
	//n = n * 125 / 65535 - 6;

	return n;
}

bool SHT30::Write(ushort cmd)
{
	ByteArray bs(0);

	return IIC->Write(cmd, bs);
}

ushort SHT30::Read2(ushort cmd)
{
	ByteArray rs(3);
	if(IIC->Read(cmd, rs) == 0) return 0;

	return (rs[0] << 8) | rs[1];
}

// 同时读取温湿度并校验Crc
uint SHT30::Read4(ushort cmd)
{
	ByteArray rs(6);
	if(IIC->Read(cmd, rs) == 0) return 0;

	// 分解数据，暂时不进行CRC校验
	byte* p = rs.GetBuffer();
	ushort temp = __REV16(*(ushort*)p);
	ushort humi = __REV16(*(ushort*)(p + 3));

	Sys.Sleep(10);

	return (temp << 16) | humi;
}
