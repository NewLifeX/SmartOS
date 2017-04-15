#include "Kernel\Sys.h"

#include "HistoryStore.h"

#define DS_DEBUG DEBUG
//#define DS_DEBUG 0
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
	StorePeriod = 600;

	MaxCache = 16 * 1024;
	MaxReport = 1024;

	OnReport = nullptr;
	OnStore = nullptr;

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
	_Store = 0;

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

	// 自动写入Store
	if (_Store <= 0) _Store = StorePeriod;
	_Store -= RenderPeriod;
	if (_Store <= 0) Store();
}

void HistoryStore::Report()
{
	if (!OnReport) return;

	auto& s = Cache;
	int len = s.Length - s.Position();
	// 没有数据
	if (!len) return;

	// 根据每次最大上报长度，计算可以上报多少条历史记录
	int n = MaxReport / Size;
	int len2 = n * Size;
	if (len2 > len) len2 = len;

	ds_printf("HistoryStore::Report %d/%d", len2, len);

	Process(len2, OnReport);
}

void HistoryStore::Store()
{
	if (!OnStore) return;

	auto& s = Cache;
	int len = s.Length - s.Position();
	// 没有数据
	if (!len) return;

	ds_printf("HistoryStore::Store %d", len);

	Process(len, OnStore);
}

void HistoryStore::Process(int len, DataHandler handler)
{
	if (!len || !handler) return;

	auto& s = Cache;

	Buffer bs(s.GetBuffer() + s.Position(), len);

	int rs = handler(this, bs, nullptr);
	if (rs) {
		s.Seek(rs);

		// 如果缓存里面没有了数据，则从头开始使用
		if (s.Position() == s.Length) {
			s.SetPosition(0);
			s.Length = 0;
		}
	}
}
