#include "Kernel\Sys.h"

#include "HistoryStore.h"

//#define DS_DEBUG DEBUG
#define DS_DEBUG 0
#if DS_DEBUG
#define ds_printf debug_printf
#else
#define ds_printf(format, ...)
#endif

// 初始化
HistoryStore::HistoryStore()
{
	Opened = false;

	RenderPeriod = 30;
	ReportPeriod = 300;
	FlashPeriod = 600;

	MaxCache = 16 * 1024;
	MaxReport = 1024;

	OnReport = nullptr;
	OnFlash = nullptr;

	_task = 0;
}

HistoryStore::~HistoryStore()
{
	Close();
}

void HistoryStore::Set(void* data, int size)
{
	Data = data;
	Size = size;
}

bool HistoryStore::Open()
{
	if (Opened) return true;

	_Report = 0;
	_Flash = 0;

	// 定时生成历史数据 30s
	int p = RenderPeriod * 1000;
	_task = Sys.AddTask(RenderTask, this, p, p, "历史数据");

	return Opened = true;
}

void HistoryStore::Close()
{
	Sys.RemoveTask(_task);

	Opened = false;
}

// 写入一条历史数据
int HistoryStore::Write(const Buffer& bs)
{
	int size = bs.Length();
	if (size == 0) return 0;

	auto& s = Cache;
	auto p = s.Position();

	// 历史数据格式：4时间 + N数据
	s.Write(Sys.Seconds());
	s.Write(bs);

	return s.Position() - p;
}

void HistoryStore::RenderTask(void* param)
{
	auto hs = (HistoryStore*)param;
	if (hs) hs->Reader();
}

void HistoryStore::Reader()
{
	// 生成历史数据
	Buffer bs(Data, Size);
	Write(bs);

	// 自动上传
	if (_Report <= 0) _Report = ReportPeriod;
	_Report -= RenderPeriod;
	if (_Report <= 0) Report();

	// 自动写入Flash
	if (_Flash <= 0) _Flash = FlashPeriod;
	_Flash -= RenderPeriod;
	if (_Flash <= 0) Flash();
}

void HistoryStore::Report()
{
	if (!OnReport) return;

	auto& s = Cache;
	// 没有数据
	if (s.Position() == s.Length) return;

	// 根据每次最大上报长度，计算可以上报多少条历史记录
	int n = MaxReport / Size;
	int len = n * Size;

	ByteArray bs(len);
	len = s.Read(bs);
	bs.SetLength(len);

	// 游标先移回去，成功以后再删除
	s.Seek(-len);

	if (len) {
		int len2 = OnReport(this, bs, nullptr);
		if (len2) {
			s.Seek(len2);

			// 如果缓存里面没有了数据，则从头开始使用
			if (s.Position() == s.Length) {
				s.SetPosition(0);
				s.Length = 0;
			}
		}
	}
}

void HistoryStore::Flash()
{
	if (!OnFlash) return;

	auto& s = Cache;
	// 没有数据
	if (s.Position() == s.Length) return;

	// 根据每次最大上报长度，计算可以上报多少条历史记录
	int n = MaxReport / Size;
	int len = n * Size;

	ByteArray bs(len);
	len = s.Read(bs);
	bs.SetLength(len);

	// 游标先移回去，成功以后再删除
	s.Seek(-len);

	if (len) {
		int len2 = OnFlash(this, bs, nullptr);
		if (len2) {
			s.Seek(len2);

			// 如果缓存里面没有了数据，则从头开始使用
			if (s.Position() == s.Length) {
				s.SetPosition(0);
				s.Length = 0;
			}
		}
	}
}
