#include "Config.h"
#include "Flash.h"
#include "Security\Crc.h"

Config* Config::Current	= NULL;

// 配置块。名称、长度、头部校验，数据部分不做校验，方便外部修改
class ConfigBlock
{
public:
	ushort	HeaderCRC;
	ushort	Size;
	char	Name[4];

    bool Valid() const;

    const ConfigBlock*	Next() const;
    const void*			Data() const;

    bool Init(const char* name, const ByteArray& bs);
    bool Write(Storage* storage, uint addr, const ByteArray& bs);
};

bool ConfigBlock::Valid() const
{
    // 计算头部 CRC。从数据CRC开始，包括大小和名称
    ushort crc = Crc::Hash16(&Size, sizeof(*this) - offsetof(ConfigBlock, Size));

    return crc == HeaderCRC;
}

const ConfigBlock* ConfigBlock::Next() const
{
    if(!Valid()) return NULL;

	// 确保数据部分2字节对齐，便于Flash操作
	uint s = (Size + 1) & ~1;

	return (const ConfigBlock*)((byte*)Data() + s);
}

// 数据所在地址，紧跟头部后面
const void* ConfigBlock::Data() const
{
    return (const void*)&this[1];
}

// 构造一个新的配置块
bool ConfigBlock::Init(const char* name, const ByteArray& bs)
{
    if(name == NULL) return false;
	uint slen = strlen(name);
    if(slen > sizeof(Name)) return false;

	Size	= bs.Length();

	if(slen > ArrayLength(Name)) slen = ArrayLength(Name);
	memset(Name, 0, ArrayLength(Name));
	memcpy(Name, name, slen);

	// 计算头部CRC。包括数据CRC、大小、名称
    HeaderCRC = Crc::Hash16(&Size, sizeof(*this) - offsetof(ConfigBlock, Size));

    return true;
}

// 更新块
bool ConfigBlock::Write(Storage* storage, uint addr, const ByteArray& bs)
{
	if(bs.Length() > Size) return false;

    bool rs = true;

	// 先写入头部，然后写入数据
	uint len = sizeof(ConfigBlock) - offsetof(ConfigBlock, HeaderCRC);
	if(!storage->Write(addr, ByteArray(&HeaderCRC, len))) return false;
	if(bs.Length() > 0)
	{
		uint len2 = bs.Length();
		if(len2 > Size) len2 = Size;
		if(!storage->Write(addr + len, ByteArray(bs.GetBuffer(), len2))) return false;
	}

    return rs;
}

//--//

Config::Config(Storage* st, uint addr)
{
	Device	= st;
	Address	= addr;
}

// 循环查找配置块
const void* Config::Find(const char* name, bool fAppend)
{
    uint c_Version = 0x534F5453; // STOS

	assert_param2(name, "配置段名称不能为空");

	uint addr = Address;
	// 检查签名，如果不存在则写入
	if(*(uint*)addr != c_Version)
	{
		if(!fAppend) return NULL;

		Device->Write(addr, ByteArray(&c_Version, sizeof(c_Version)));
	}

	addr += sizeof(c_Version);
    const ConfigBlock* cfg = (const ConfigBlock*)addr;
	uint slen = strlen(name);
	assert_param2(slen <= sizeof(cfg->Name), "配置段名称最大4个字符");

	// 遍历链表，找到同名块
    while(cfg->Valid())
    {
        if(cfg->Name[0] && memcmp(name, cfg->Name, slen) == 0) return cfg;

        cfg = cfg->Next();
    }

	// 如果需要添加，返回最后一个非法块的地址
    return fAppend ? cfg : NULL;
}

// 废弃
bool Config::Invalid(const char* name)
{
    return Set(name, ByteArray(0));
}

// 根据名称更新块
const void* Config::Set(const char* name, const ByteArray& bs)
{
    if(name == NULL) return NULL;

	assert_param2(Device, "未指定配置段的存储设备");

	const ConfigBlock* cfg = (const ConfigBlock*)Find(name, bs.Length() > 0);
    if(cfg)
	{
		// 重新搞一个配置头，使用新的数据去重新初始化
		ConfigBlock header;
		header.Init(name, bs);
		header.Write(Device, (uint)cfg, bs);

		return cfg->Data();
	}

    return NULL;
}

// 获取配置数据
bool Config::Get(const char* name, ByteArray& bs)
{
    if(name == NULL) return false;

	const ConfigBlock* cfg = (const ConfigBlock*)Find(name, false);
    if(cfg && cfg->Size > 0 && cfg->Size <= bs.Capacity())
	{
		bs.Copy(cfg->Data(), 0, cfg->Size);
		bs.SetLength(cfg->Size);

		return true;
    }

    return false;
}

const void* Config::Get(const char* name)
{
    if(name == NULL) return NULL;

	const ConfigBlock* cfg = (const ConfigBlock*)Find(name, false);
    if(cfg && cfg->Size) return cfg->Data();

    return NULL;
}

// Flash最后一块作为配置区
Config& Config::CreateFlash()
{
	// 最后一块作为配置区
	static Flash flash;
	static Config cfg(&flash, flash.Start + flash.Size - flash.Block);
	//cfg.Device		= &flash;
	//cfg.Address		= flash.Start + flash.Size - flash.Block;

	return cfg;
}

// RAM最后一小段作为热启动配置区
Config& Config::CreateRAM()
{
	// 最后一块作为配置区
	static CharStorage cs;
	static Config cfg(&cs, Sys.StackTop());
	//cfg.Device		= &cs;
	//cfg.Address		= Sys.StackTop();

	return cfg;
}

void* HotConfig::Next() const
{
	return (void*)&this[1];
}

HotConfig& HotConfig::Current()
{
	static Config& cfg = Config::CreateRAM();

	// 查找配置数据，如果不存在，则清空
	const void* dat = cfg.Get("Hot");
	if(!dat) dat = cfg.Set("Hot", ByteArray((byte)0, sizeof(HotConfig)));

	return *(HotConfig*)dat;
}

// 初始化
TConfig::TConfig()
{
	// 实际内存大小，减去头部大小
	uint len = sizeof(this) - ((int)&Length - (int)this);
	//memset(&Length, 0, len);
	Length = len;
}

void TConfig::LoadDefault()
{
	Kind	= Sys.Code;
	//Server	= 0x01;

	PingTime	= 15;
	OfflineTime	= 60;
}
