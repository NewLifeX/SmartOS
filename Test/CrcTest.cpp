#include "Sys.h"
#include "Security\Crc.h"
#include "conf.h"

static const uint DataBuffer[] =
{
    0x00001021, 0x20423063, 0x408450a5, 0x60c670e7, 0x9129a14a, 0xb16bc18c,
    0xd1ade1ce, 0xf1ef1231, 0x32732252, 0x52b54294, 0x72f762d6, 0x93398318,
    0xa35ad3bd, 0xc39cf3ff, 0xe3de2462, 0x34430420, 0x64e674c7, 0x44a45485,
    0xa56ab54b, 0x85289509, 0xf5cfc5ac, 0xd58d3653, 0x26721611, 0x063076d7,
    0x569546b4, 0xb75ba77a, 0x97198738, 0xf7dfe7fe, 0xc7bc48c4, 0x58e56886,
    0x78a70840, 0x18612802, 0xc9ccd9ed, 0xe98ef9af, 0x89489969, 0xa90ab92b,
    0x4ad47ab7, 0x6a961a71, 0x0a503a33, 0x2a12dbfd, 0xfbbfeb9e, 0x9b798b58,
    0xbb3bab1a, 0x6ca67c87, 0x5cc52c22, 0x3c030c60, 0x1c41edae, 0xfd8fcdec,
    0xad2abd0b, 0x8d689d49, 0x7e976eb6, 0x5ed54ef4, 0x2e321e51, 0x0e70ff9f,
    0xefbedfdd, 0xcffcbf1b, 0x9f598f78, 0x918881a9, 0xb1caa1eb, 0xd10cc12d,
    0xe16f1080, 0x00a130c2, 0x20e35004, 0x40257046, 0x83b99398, 0xa3fbb3da,
    0xc33dd31c, 0xe37ff35e, 0x129022f3, 0x32d24235, 0x52146277, 0x7256b5ea,
    0x95a88589, 0xf56ee54f, 0xd52cc50d, 0x34e224c3, 0x04817466, 0x64475424,
    0x4405a7db, 0xb7fa8799, 0xe75ff77e, 0xc71dd73c, 0x26d336f2, 0x069116b0,
    0x76764615, 0x5634d94c, 0xc96df90e, 0xe92f99c8, 0xb98aa9ab, 0x58444865,
    0x78066827, 0x18c008e1, 0x28a3cb7d, 0xdb5ceb3f, 0xfb1e8bf9, 0x9bd8abbb,
    0x4a755a54, 0x6a377a16, 0x0af11ad0, 0x2ab33a92, 0xed0fdd6c, 0xcd4dbdaa,
    0xad8b9de8, 0x8dc97c26, 0x5c644c45, 0x3ca22c83, 0x1ce00cc1, 0xef1fff3e,
    0xdf7caf9b, 0xbfba8fd9, 0x9ff86e17, 0x7e364e55, 0x2e933eb2, 0x0ed11ef0
};

bool inited = false;

void Init()
{
#ifdef STM32F4
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_CRC, ENABLE);
#else
  RCC->AHBENR |= RCC_AHBPeriph_CRC;
#endif

  inited = true;
}

// 硬件实现的Crc
uint HardCrc(const Array& bs, uint crc)
{
	if (!inited) Init();

    CRC_ResetDR();
    // STM32的初值是0xFFFFFFFF，而软Crc初值是0
	CRC->DR		= _REV(crc ^ 0xFFFFFFFF);
    //CRC->DR = 0xFFFFFFFF;
    uint* ptr	= (uint*)bs.GetBuffer();
	uint len	= bs.Length();
    len >>= 2;
    while(len-- > 0)
    {
        CRC->DR =_REV(*ptr++);    // 字节顺序倒过来,注意不是位序,不是用__RBIT指令
    }
    return CRC->DR;
	/*byte* ptr = (byte*)buf;
    while(len-- > 0)
    {
        CRC->DR = (uint)*ptr++;
    }
    return CRC->DR;*/

	/*CRC_CalcBlockCRC(ptr, len);
	return CRC_GetCRC();*/

    //UINT32 rs = CRC->DR;
    //return c_CRCTable[ ((crc >> 24) ^ rs) & 0xFF ] ^ (crc << 8);
}

void TestCrc()
{
    debug_printf("\r\n\r\n");
    debug_printf("TestCrc Start......\r\n");
    debug_printf("\r\n");

    uint size	= ArrayLength(DataBuffer);

	// Sys.Crc是软校验，HardCrc是硬件实现，要求硬件实现的结果跟软件实现一致
	uint data	= 0x12345678;
	ByteArray bs(&data, 4);
	uint crc	= Crc::Hash(bs, 0);
	uint crc2	= Crc::Hash(bs, 4);
	bs.Show();
	debug_printf("\r\n\tSoftCrc:0x%08x  HardCrc:0x%08x \r\n", crc, crc2);
	// 无初值时，两者一样

	uint temp	= crc;
	// 试试二次计算Crc
	crc		= Crc::Hash(Array(&crc, 4), 0);
	crc2	= Crc::Hash(Array(&crc2, 4));
	ByteArray(&temp, 4).Show();
	debug_printf("\r\n\t");
	debug_printf("SoftCrc:0x%08x  HardCrc:0x%08x \r\n", crc, crc2);
	// 结果相同，但都不是0

	// 连续测试。构建8字节，前面是data，后面是前面的crc
	ulong data2 = temp;
	data2	<<= 32;
	data2	+= data;
	ByteArray bs2(&data2, 8);
	crc		= Crc::Hash(bs2, 0);
	crc2	= Crc::Hash(bs2);
	bs2.Show();
	debug_printf("\r\n\t");
	debug_printf("SoftCrc:0x%08x  HardCrc:0x%08x \r\n", crc, crc2);
	// 结果相同，但都不是0

	// 实际应用中，先计算数据的校验，然后接着附加校验码部分，跟直接连续计算效果应该一致
	// 实际上就是数字为初值，对它自身进行校验码计算
	ByteArray bs3(&temp, 4);
	crc		= Crc::Hash(bs3, data);
	crc2	= HardCrc(bs3, data);
	bs3.Show();
	debug_printf(" <= 0x%08x\r\n\t", data);
	debug_printf("SoftCrc:0x%08x  HardCrc:0x%08x \r\n", crc, crc2);
	// 结果不同，HardCrc结果跟8字节测试相同

	ByteArray bs4(&temp, 4);
	crc		= Crc::Hash(bs4, temp);
	crc2	= HardCrc(bs4, temp);
	bs4.Show();
	debug_printf(" <= 0x%08x\r\n\t", temp);
	debug_printf("SoftCrc:0x%08x  HardCrc:0x%08x \r\n", crc, crc2);
	// 结果不同，SoftCrc结果跟8字节测试相同

	// 对大数据块进行校验
    debug_printf("\r\n");

	ByteArray bs5(DataBuffer, size*4);
	crc		= Crc::Hash(bs5, 0);
	crc2	= Crc::Hash(bs5);
	bs5.Show();
	debug_printf("\r\n\t");
	debug_printf("SoftCrc:0x%08x  HardCrc:0x%08x \r\n", crc, crc2);
	// 无初值时，两者一样

	temp = crc;

	// 实际应用中，先计算数据的校验，然后接着附加校验码部分
	ByteArray bs6(&temp, 4);
	crc		= Crc::Hash(bs6, temp);
	crc2	= HardCrc(bs6, temp);
	bs6.Show();
	debug_printf(" <= 0x%08x\r\n\t", temp);
	debug_printf("SoftCrc:0x%08x  HardCrc:0x%08x \r\n", crc, crc2);
	// 有初值时，两者不一样

	// 增量计算CRC
	ByteArray bs7(DataBuffer, size*4);
	crc		= Crc::Hash(bs7, 0);
	temp	= crc;
	crc		= Crc::Hash(Array(&crc, 4), crc);
	crc2	= HardCrc(bs7, 0);
	crc2	= HardCrc(Array(&crc2, 4), crc2);
	bs7.Show();
	debug_printf(" <= 0x%08x\r\n\t", temp);
	debug_printf("SoftCrc:0x%08x  HardCrc:0x%08x \r\n", crc, crc2);

	// 测试Crc16，数据和crc部分一起计算crc16，结果为0
    debug_printf("\r\n");
    byte data16[] = { 0x01, 0x08, 0x00, 0x00};
    ushort crc16 = Crc::Hash16(Array(data16, 4));
    debug_printf("Crc::Hash16(#%08x) = 0x%04x\r\n", _REV(*(uint*)data16), crc16);
    ushort crc17 = Crc::Hash16(Array(&crc16, 2), crc16);
    debug_printf("Crc::Hash16(#%08x, 0x%04x) = 0x%04x\r\n", _REV(*(uint*)data16), crc16, crc17);

    debug_printf("\r\n");
    debug_printf("TestCrc Finish!\r\n");
}
