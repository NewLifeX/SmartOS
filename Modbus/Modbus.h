#ifndef __Slave_H__
#define __Slave_H__

#include "Sys.h"
#include "Stream.h"

class ModbusErrors
{
public:
	// 错误代码
	enum Errors
	{
		// 错误的功能代码
		Code = 1,

		// 错误的数据地址
		Address = 2,

		// 错误的数据值
		Value = 3,

		// 错误的个数
		Count,

		// 处理出错
		Process,

		// 错误的数据长度
		Length,

		// Crc校验错误
		Crc
	};
};

// Modbus消息实体
class Modbus
{
private:

public:
	byte	Address;	// 地址
	byte	Code;		// 功能码
	byte	Error;		// 是否异常

	byte	Length;		// 数据长度
	byte	Data[32];	// 数据

	ushort	Crc;		// 校验码
	ushort	Crc2;		// 动态计算得到的校验码

	Modbus();

	bool Read(MemoryStream& ms);
	void Write(MemoryStream& ms);
	
	void SetError(ModbusErrors::Errors error);
private:
};

#endif

/*
 * GB/T 19582.1-2008 基于Modbus协议的工业自动化网络规范
 * 请求响应：1字节功能码|n字节数据|2字节CRC校验
 * 异常响应：1字节功能码+0x80|1字节异常码
 *
 * Modbus数据模型基本表
 * 基本表        对象类型   访问类型    注释
 * 离散量输入    单个位     只读        I/O系统可提供这种类型的数据
 * 线圈          单个位     读写        通过应用程序可改变这种类型的数据
 * 输入寄存器    16位字     只读        I/O系统可提供这种类型的数据
 * 保持寄存器    16位字     读写        通过应用程序可改变这种类型的数据
 *
 */
