#include "Config.h"
#include "Security\Crc.h"

Storage*	ConfigBlock::Device			= NULL;
uint		ConfigBlock::BaseAddress	= NULL;

bool ConfigBlock::IsGoodBlock() const
{
    if(Signature != c_Version || HeaderCRC == 0) return false;

    // 计算头部 CRC。从数据CRC开始，包括大小和名称
    ushort crc = Crc::Hash16(&DataCRC, sizeof(*this) - offsetof(ConfigBlock, DataCRC));

    return crc == HeaderCRC;
}

bool ConfigBlock::IsGoodData() const
{
    // 计算数据块CRC
    ushort crc = Crc::Hash16(Data(), Size);

    return crc == DataCRC;
}

const ConfigBlock* ConfigBlock::Next() const
{
    if(!IsGoodBlock()) return NULL;

	// 确保数据部分2字节对齐，便于Flash操作
	uint s = (Size + 1) & ~1;

	return (const ConfigBlock*)((byte*)Data() + s);
}

// 数据所在地址，紧跟头部后面
const void* ConfigBlock::Data() const
{
    return (const void*)&this[1];
}

//--//

// 构造一个新的配置块
bool ConfigBlock::Init(const char* name, const ByteArray& bs)
{
    if(name == NULL) return false;
	uint slen = strlen(name);
    if(slen > sizeof(Name)) return false;

    //memset(this, 0, sizeof(*this));

	// 数据长度为0，表明是置为无效
	if(bs.Length() > 0)
		DataCRC	= Crc::Hash16(bs.GetBuffer(), bs.Length());
	else
		DataCRC	= 0;

	/*// 只有首次允许设置名字和大小，避免后面随意扩张引起越界覆盖
	if(IsGoodBlock())
	{
		if(bs.Length() > Size) return false;
	}
	else*/
	{
		Signature = c_Version;
		Size      = bs.Length();

		if(slen > ArrayLength(Name)) slen = ArrayLength(Name);
		memset(Name, 0, ArrayLength(Name));
		memcpy(Name, name, slen);
	}

	// 计算头部CRC。包括数据CRC、大小、名称
    HeaderCRC = Crc::Hash16(&DataCRC, sizeof(*this) - offsetof(ConfigBlock, DataCRC));

    return true;
}

//--//

// 循环查找配置块
const ConfigBlock* ConfigBlock::Find(const char* name, bool fAppend) const
{
    const ConfigBlock* ptr = this;

	// 遍历链表，找到同名块
    while(ptr->IsGoodBlock())
    {
        if(ptr->IsGoodData() && name && strcmp(name, ptr->Name) == 0)
        {
            return ptr;
        }

        ptr = ptr->Next();
    }

	// 如果需要添加，返回最后一个非法块的地址
    return fAppend ? ptr : NULL;
}

// 更新块
bool ConfigBlock::Write(Storage* storage, uint addr, const ByteArray& bs)
{
	if(bs.Length() > Size) return false;

    bool rs = true;

	// 先写入头部，然后写入数据
	uint len = sizeof(ConfigBlock) - offsetof(ConfigBlock, Signature);
	rs &= storage->Write(addr, ByteArray((byte*)&Signature, len));
	if(bs.Length() > 0)
	{
		uint offset = len;
		len = bs.Length();
		if(len > Size) len = Size;
		rs &= storage->Write(addr + offset, ByteArray(bs.GetBuffer(), len));
	}

    return rs;
}

// 废弃
bool ConfigBlock::Invalid(const char* name, uint addr, Storage* storage)
{
    return Set(name, ByteArray(0), addr, storage);
}

// 根据名称更新块
const void* ConfigBlock::Set(const char* name, const ByteArray& bs, uint addr, Storage* storage)
{
    if(name == NULL) return NULL;

	if(!addr) addr = BaseAddress;
	if(!storage) storage = Device;

	const ConfigBlock* cfg = (const ConfigBlock*)addr;

    if(cfg) cfg = cfg->Find(name, true);
    if(cfg)
	{
		// 重新搞一个配置头，使用新的数据去重新初始化
		ConfigBlock header;
		header.Init(name, bs);
		header.Write(storage, (uint)cfg, bs);

		return cfg->Data();
	}

    return NULL;
}

// 获取配置数据
bool ConfigBlock::Get(const char* name, ByteArray& bs, uint addr)
{
    if(name == NULL) return false;

	if(!addr) addr = BaseAddress;
	const ConfigBlock* cfg = (const ConfigBlock*)addr;

    if(cfg) cfg = cfg->Find(name, false);
    if(cfg)
    {
        if(cfg->Size <= bs.Capacity())
        {
			bs.Copy((byte*)cfg->Data(), 0, cfg->Size);
			bs.SetLength(cfg->Size);

            return true;
        }
    }

    return false;
}

const void* ConfigBlock::Get(const char* name, uint addr)
{
    if(name == NULL) return NULL;

	if(!addr) addr = BaseAddress;
	const ConfigBlock* cfg = (const ConfigBlock*)addr;

    if(cfg) cfg = cfg->Find(name, false);
    if(cfg) return cfg->Data();

    return NULL;
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
