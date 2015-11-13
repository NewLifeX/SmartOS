#include "Crc.h"

static const ushort c_CRC16Table[] =
{
0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401,
0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400,
};

ushort Crc::Hash16(const Array& arr, ushort crc)
{
    const byte* buf	= arr.GetBuffer();
	int len			= arr.Length();

	assert_param2(buf, "Crc16校验目标地址不能为空");
    if (!buf || !len) return 0;

    for (int i = 0; i < len; i++)
    {
        byte b = buf[i];
        crc = (ushort)(c_CRC16Table[(b ^ crc) & 0x0F] ^ (crc >> 4));
        crc = (ushort)(c_CRC16Table[((b >> 4) ^ crc) & 0x0F] ^ (crc >> 4));
    }
    return crc;
}
