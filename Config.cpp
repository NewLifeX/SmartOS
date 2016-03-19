#include "Config.h"
#include "Flash.h"
#include "Security\Crc.h"

#define CFG_DEBUG DEBUG
//#define CFG_DEBUG 0
#if CFG_DEBUG
	//#define CTS TS
#else
	#undef TS
	#define TS(name)
#endif

const Config* Config::Current = nullptr;

/*================================ 配置块 ================================*/

// 配置块。名称、长度、头部校验，数据部分不做校验，方便外部修改
struct ConfigBlock
{
	ushort	Hash;
	ushort	Size;
	char	Name[8];	// 零结尾字符串

	ushort GetHash() const;
    bool Valid() const;

    const ConfigBlock*	Next() const;
    const void*			Data() const;
	uint CopyTo(Buffer& bs) const;

    bool Init(const String& name, const Buffer& bs);
    bool Write(const Storage& storage, uint addr, const Buffer& bs);
	bool Remove(const Storage& storage, uint addr);
};

ushort ConfigBlock::GetHash() const
{
    // 计算头部 CRC。从数据CRC开始，包括大小和名称
    return Crc::Hash16(Buffer((byte*)&Size, sizeof(*this) - offsetof(ConfigBlock, Size)));
}

bool ConfigBlock::Valid() const
{
    return GetHash() == Hash;
}

// 获取下一块。当前块必须有效，否则返回空，下一块不在乎有效无效
const ConfigBlock* ConfigBlock::Next() const
{
    if(!Valid()) return nullptr;

	// 确保数据部分2字节对齐，便于Flash操作
	uint s = (Size + 1) & 0xFFFE;

	return (const ConfigBlock*)((byte*)Data() + s);
}

// 数据所在地址，紧跟头部后面
const void* ConfigBlock::Data() const
{
    return (const void*)&this[1];
}

uint ConfigBlock::CopyTo(Buffer& bs) const
{
    if(Size == 0 || Size > bs.Length()) return 0;

	return bs.Copy(0, Data(), Size);
}

// 构造一个新的配置块
bool ConfigBlock::Init(const String& name, const Buffer& bs)
{
    if(!name) return false;

	assert(name.Length() < sizeof(Name), "配置区名称太长");
    if(name.Length() >= sizeof(Name)) return false;

	//TS("ConfigBlock::Init");

	// 配置块的大小，只有第一次能够修改，以后即使废弃也不能修改，仅仅清空名称
	if(bs.Length() > 0)
	{
		Size	= bs.Length();
		name.CopyTo(0, Name, -1);
	}

    Hash = GetHash();

    return true;
}

// 更新块
bool ConfigBlock::Write(const Storage& storage, uint addr, const Buffer& bs)
{
	TS("ConfigBlock::Write");

	// 如果大小超标，并且下一块有效，那么这是非法操作
	if(bs.Length() > Size && Next()->Valid())
	{
		debug_printf("ConfigBlock::Write 配置块 %s 大小 %d < %d \r\n", Name, Size, bs.Length());
		return false;
	}

    Hash = GetHash();

	// 先写入头部，然后写入数据
	uint len = sizeof(ConfigBlock) - offsetof(ConfigBlock, Hash);
	if(!storage.Write(addr, Buffer(&Hash, len))) return false;
	if(bs.Length() > 0)
	{
		uint len2 = bs.Length();
		if(len2 > Size) len2 = Size;
		if(!storage.Write(addr + len, bs.Sub(0, len2))) return false;
	}

    return true;
}

bool ConfigBlock::Remove(const Storage& storage, uint addr)
{
	// 把整个名称区域清空
	Buffer(Name, ArrayLength(Name)).Clear();

    Hash = GetHash();

	// 写入头部
	uint len = sizeof(ConfigBlock) - offsetof(ConfigBlock, Hash);
	return storage.Write(addr, Buffer(&Hash, len));
}

/*================================ 配置 ================================*/

Config::Config(const Storage& st, uint addr, uint size)
	: Device(st)
{
	Address	= addr;
	Size	= size;
}

// 检查签名
bool CheckSignature(const Storage& st, uint& addr, bool create)
{
    const uint c_Version = 0x534F5453; // STOS

	// 检查签名，如果不存在则写入
	if(*(uint*)addr != c_Version)
	{
		if(!create) return false;

		st.Write(addr, Buffer((byte*)&c_Version, sizeof(c_Version)));
	}

	addr += sizeof(c_Version);

	return true;
}

// 循环查找配置块
const ConfigBlock* FindBlock(const Storage& st, uint addr, const String& name)
{
	TS("Config::Find");

    if(!name) return nullptr;

	assert(name.Length() < sizeof(ConfigBlock::Name), "配置区名称太长");
    if(name.Length() >= sizeof(ConfigBlock::Name)) return nullptr;

	if(!CheckSignature(st, addr, false)) return nullptr;

	// 第一个配置块
    auto cfg = (const ConfigBlock*)addr;

	// 遍历链表，找到同名块
    while(cfg->Valid())
    {
        if(cfg->Name[0] && name == cfg->Name) return cfg;

        cfg = cfg->Next();
    }

	return nullptr;
}

// 创建一个指定大小的配置块。找一个满足该大小的空闲数据块，或者在最后划分一个
const ConfigBlock* NewBlock(const Storage& st, uint addr, int size)
{
	TS("Config::New");

	if(!CheckSignature(st, addr, true)) return nullptr;

	// 第一个配置块
    auto cfg = (const ConfigBlock*)addr;

	// 找一块合适大小的空闲区域
    while(cfg->Valid())
    {
        if(cfg->Name[0] == 0 && cfg->Size == size) return cfg;

        cfg = cfg->Next();
    }

	return cfg;
}

// 循环查找配置块
const void* Config::Find(const String& name) const
{
	return FindBlock(Device, Address, name);
}

// 创建一个指定大小的配置块。找一个满足该大小的空闲数据块，或者在最后划分一个
const void* Config::New(int size) const
{
	auto cfg	= NewBlock(Device, Address, size);

	// 实在没办法，最后划分一个新的区块。这里判断一下空间是否足够
	if(Size && (uint)cfg + sizeof(ConfigBlock) + size > Address + Size)
	{
		debug_printf("Config::New 0x%08X + %d + %d 配置区（0x%08X, %d）空间不足\r\n", cfg, sizeof(ConfigBlock), size, Address, Size);

		return nullptr;
	}

	return cfg;
}

// 删除
bool Config::Remove(const String& name) const
{
    //return Set(name, ByteArray(0));
	TS("Config::Remove");

    if(!name) return false;

	auto cfg = FindBlock(Device, Address, name);
	if(!cfg) return false;

	// 只清空名称，修改哈希，不能改大小，否则无法定位下一个配置块
	// 拷贝一份修改
	auto header	= *cfg;

	return header.Remove(Device, (uint)cfg);
}

// 根据名称更新块
const void* Config::Set(const String& name, const Buffer& bs) const
{
	TS("Config::Set");

    if(!name) return nullptr;

	assert(name.Length() < sizeof(ConfigBlock::Name), "配置区名称太长");
    if(name.Length() >= sizeof(ConfigBlock::Name)) return nullptr;

	auto cfg = FindBlock(Device, Address, name);
	if(!cfg) cfg	= NewBlock(Device, Address, bs.Length());
    if(!cfg) return nullptr;

	// 重新搞一个配置头，使用新的数据去重新初始化
	ConfigBlock header;
	header.Init(name, bs);
	if(!header.Write(Device, (uint)cfg, bs)) return nullptr;

	return cfg->Data();
}

// 获取配置数据
bool Config::Get(const String& name, Buffer& bs) const
{
	TS("Config::Get");

    if(!name) return false;

	auto cfg = FindBlock(Device, Address, name);
    if(!cfg) return false;

	return cfg->CopyTo(bs) > 0;
}

const void* Config::Get(const String& name) const
{
	TS("Config::GetByName");

    if(!name) return nullptr;

	auto cfg = FindBlock(Device, Address, name);
    if(cfg && cfg->Size) return cfg->Data();

    return nullptr;
}

/*// 获取配置数据，如果不存在则覆盖
bool Config::GetOrSet(const String& name, Buffer& bs) const
{
	TS("Config::GetOrSet");

    if(name == nullptr) return false;
    //assert(name, "配置块名称不能为空");

	// 输入数据已存在，直接返回
	if(Get(name, bs)) return true;

	// 否则，用这块数据去覆盖吧
	Set(name, bs);

    return false;
}*/

// Flash最后一块作为配置区
const Config& Config::CreateFlash()
{
	// 最后一块作为配置区
	static Flash flash;
	static Config cfg(flash, flash.Start + flash.Size - flash.Block, flash.Block);

	return cfg;
}

// RAM最后一小段作为热启动配置区
const Config& Config::CreateRAM()
{
	// 最后一块作为配置区
	static CharStorage cs;
	static Config cfg(cs, Sys.StackTop(), 64);

	return cfg;
}

/******************************** ConfigBase ********************************/

ConfigBase::ConfigBase()
	: Cfg(*Config::Current)
{
	assert_ptr(&Cfg);

	New	= true;

	_Name	= nullptr;
}

uint ConfigBase::Size() const
{
	assert(_End && _Start, "_Start & _End == nullptr");

	return (uint)_End - (uint)_Start;
}

Buffer ConfigBase::ToArray()
{
	return Buffer(_Start, Size());
}

const Buffer ConfigBase::ToArray() const
{
	return Buffer(_Start, Size());
}

void ConfigBase::Init()
{
	Buffer(_Start, Size()).Clear();
}

void ConfigBase::Load()
{
	// 尝试加载配置区设置
	auto bs	= ToArray();
	//New		= !Cfg.GetOrSet(_Name, bs);
	New	= !Cfg.Get(_Name, bs);
	if(New)
		debug_printf("%s::Load 首次运行，创建配置区！\r\n", _Name);
	else
		debug_printf("%s::Load 从配置区加载配置 %d 字节\r\n", _Name, bs.Length());
}

void ConfigBase::Save() const
{
	//auto bs	= ToArray();
	Buffer bs(_Start, Size());
	debug_printf("%s::Save %d 字节 ", _Name, bs.Length());

	auto pt	= Cfg.Set(_Name, bs);
	if(pt)
		debug_printf("成功 0x%08X \r\n", pt);
	else
		debug_printf("失败\r\n");
}

void ConfigBase::Clear()
{
	debug_printf("%s::Clear ", _Name);

	bool rs	= Cfg.Remove(_Name);
	if(rs)
		debug_printf("成功\r\n");
	else
		debug_printf("失败\r\n");
}

void ConfigBase::Show() const
{
}

void ConfigBase::Write(Stream& ms) const
{
	ms.Write(ToArray());
}

void ConfigBase::Read(Stream& ms)
{
	auto bs	= ToArray();
	ms.Read(bs);
}

/******************************** HotConfig ********************************/

void* HotConfig::Next() const
{
	return (void*)&this[1];
}

HotConfig& HotConfig::Current()
{
	static const Config& cfg = Config::CreateRAM();

	// 查找配置数据，如果不存在，则清空
	auto dat = cfg.Get("Hot");
	if(!dat)
	{
		ByteArray bs(sizeof(HotConfig));
		bs.Set(0, 0, bs.Length());
		dat = cfg.Set("Hot", bs);
	}

	return *(HotConfig*)dat;
}
