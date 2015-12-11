#include "Config.h"
#include "Flash.h"
#include "Security\Crc.h"

//#define CFG_DEBUG DEBUG
#define CFG_DEBUG 0
#if CFG_DEBUG
	//#define CTS TS
#else
	#undef TS
	#define TS(name)
#endif

Config* Config::Current	= NULL;

// 配置块。名称、长度、头部校验，数据部分不做校验，方便外部修改
struct ConfigBlock
{
	ushort	HeaderCRC;
	ushort	Size;
	char	Name[4];

	ushort GetHash() const;
    bool Valid() const;

    const ConfigBlock*	Next() const;
    const void*			Data() const;

    bool Init(const char* name, const Array& bs);
    bool Write(Storage* storage, uint addr, const Array& bs);
};

ushort ConfigBlock::GetHash() const
{
    // 计算头部 CRC。从数据CRC开始，包括大小和名称
    return Crc::Hash16(Array(&Size, sizeof(*this) - offsetof(ConfigBlock, Size)));
}

bool ConfigBlock::Valid() const
{
    return GetHash() == HeaderCRC;
}

const ConfigBlock* ConfigBlock::Next() const
{
    if(!Valid()) return NULL;

	// 确保数据部分2字节对齐，便于Flash操作
	uint s = (Size + 1) & 0xFFFE;

	return (const ConfigBlock*)((byte*)Data() + s);
}

// 数据所在地址，紧跟头部后面
const void* ConfigBlock::Data() const
{
    return (const void*)&this[1];
}

// 构造一个新的配置块
bool ConfigBlock::Init(const char* name, const Array& bs)
{
    if(name == NULL) return false;
    //assert_param2(name, "配置块名称不能为空");

	TS("ConfigBlock::Init");

	uint slen = strlen(name);
    if(slen > sizeof(Name)) return false;

	//Size	= bs.Length();

	if(slen > ArrayLength(Name)) slen = ArrayLength(Name);
	memset(Name, 0, ArrayLength(Name));

	// 配置块的大小，只有第一次能够修改，以后即使废弃也不能修改，仅仅清空名称
	if(bs.Length() > 0)
	{
		Size	= bs.Length();
		memcpy(Name, name, slen);
	}

    HeaderCRC = GetHash();

    return true;
}

// 更新块
bool ConfigBlock::Write(Storage* storage, uint addr, const Array& bs)
{
	assert_ptr(storage);

	TS("ConfigBlock::Write");

	// 如果大小超标，并且下一块有效，那么这是非法操作
	if(bs.Length() > Size && Next()->Valid())
	{
		debug_printf("ConfigBlock::Write 配置块 %s 大小 %d 小于要写入的数据库大小 %d，并且下一块是有效配置块，不能覆盖！ \r\n", Name, Size, bs.Length());
		return false;
	}

    bool rs = true;

	// 先写入头部，然后写入数据
	uint len = sizeof(ConfigBlock) - offsetof(ConfigBlock, HeaderCRC);
	if(!storage->Write(addr, Array(&HeaderCRC, len))) return false;
	if(bs.Length() > 0)
	{
		uint len2 = bs.Length();
		if(len2 > Size) len2 = Size;
		if(!storage->Write(addr + len, Array(bs.GetBuffer(), len2))) return false;
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
const void* Config::Find(const char* name, int size)
{
	TS("Config::Find");

    const uint c_Version = 0x534F5453; // STOS

    if(name == NULL) return NULL;
	//assert_param2(name, "配置段名称不能为空");

	uint addr = Address;
	// 检查签名，如果不存在则写入
	if(*(uint*)addr != c_Version)
	{
		if(!size) return NULL;

		Device->Write(addr, Array(&c_Version, sizeof(c_Version)));
	}

	addr += sizeof(c_Version);
    auto cfg = (const ConfigBlock*)addr;
	uint slen = strlen(name);
    if(slen > sizeof(cfg->Name)) return NULL;
	//assert_param2(slen <= sizeof(cfg->Name), "配置段名称最大4个字符");

	// 遍历链表，找到同名块
    while(cfg->Valid())
    {
        if(cfg->Name[0] && memcmp(name, cfg->Name, slen) == 0) return cfg;

        cfg = cfg->Next();
    }

	// 如果需要添加，返回最后一个非法块的地址
    //return fAppend ? cfg : NULL;

	if(!size) return NULL;

	// 找一块合适的区域
    while(cfg->Valid())
    {
        if(cfg->Name[0] && cfg->Size == size) return cfg;

        cfg = cfg->Next();
    }

	return cfg;
}

// 废弃
bool Config::Invalid(const char* name)
{
    return Set(name, ByteArray(0));
}

// 根据名称更新块
const void* Config::Set(const char* name, const Array& bs)
{
	TS("Config::Set");

    if(name == NULL) return NULL;
    //assert_param2(name, "配置块名称不能为空");
	//assert_param2(Device, "未指定配置段的存储设备");

	auto cfg = (const ConfigBlock*)Find(name, bs.Length());
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
bool Config::Get(const char* name, Array& bs)
{
	TS("Config::Get");

    if(name == NULL) return false;
    //assert_param2(name, "配置块名称不能为空");

	auto cfg = (const ConfigBlock*)Find(name, 0);
    if(cfg && cfg->Size > 0 && cfg->Size <= bs.Capacity())
	{
		bs.Copy(cfg->Data(), cfg->Size);
		bs.SetLength(cfg->Size);

		return true;
    }

    return false;
}

// 获取配置数据，如果不存在则覆盖
bool Config::GetOrSet(const char* name, Array& bs)
{
	TS("Config::GetOrSet");

    if(name == NULL) return false;
    //assert_param2(name, "配置块名称不能为空");

	// 输入数据已存在，直接返回
	if(Get(name, bs)) return true;

	// 否则，用这块数据去覆盖吧
	Set(name, bs);

    return false;
}

const void* Config::Get(const char* name)
{
	TS("Config::GetByName");

    if(name == NULL) return NULL;
    //assert_param2(name, "配置块名称不能为空");

	auto cfg = (const ConfigBlock*)Find(name, 0);
    if(cfg && cfg->Size) return cfg->Data();

    return NULL;
}

// Flash最后一块作为配置区
Config& Config::CreateFlash()
{
	// 最后一块作为配置区
	static Flash flash;
	static Config cfg(&flash, flash.Start + flash.Size - flash.Block);

	return cfg;
}

// RAM最后一小段作为热启动配置区
Config& Config::CreateRAM()
{
	// 最后一块作为配置区
	static CharStorage cs;
	static Config cfg(&cs, Sys.StackTop());

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
	auto dat = cfg.Get("Hot");
	if(!dat) dat = cfg.Set("Hot", ByteArray((byte)0, sizeof(HotConfig)));

	return *(HotConfig*)dat;
}
