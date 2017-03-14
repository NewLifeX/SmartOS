#include "Kernel\Sys.h"
#include "Device\Port.h"

#include "ACZero.h"

static void ACZeroTask(void *param);

ACZero::ACZero()
{
	Time = 0;
	//AdjustTime = 2;

	_taskid = 0;
}

ACZero::~ACZero()
{
	if (Port.Opened) Close();
	Sys.RemoveTask(_taskid);
}

void ACZero::Set(Pin pin)
{
	if (pin == P0) return;

	Port.Set(pin);
}

void ACZeroTask(void *param)
{
	auto port = (ACZero*)param;
	if (port)
	{
		debug_printf("定时检查交流电过零\r\n");

		// 需要检测是否有交流电，否则关闭
		port->Check();
	}
}

bool ACZero::Open()
{
	if (!Port.Open()) return false;

	debug_printf("过零检测引脚探测: ");

	if (Check())
	{
		debug_printf("已连接交流电！\r\n");
	}
	else
	{
		debug_printf("未连接交流电！\r\n");
		Port.Close();
		return false;
	}

	_taskid = Sys.AddTask(ACZeroTask, this, 1000, 10000, "过零检测");

	return true;
}

void ACZero::Close()
{
	Port.Close();
}

bool ACZero::Check()
{
	// 检测下降沿   先去掉低电平  while（io==false）
	int retry = 200;
	while (Port == false && retry-- > 0) Sys.Delay(100);
	if (retry <= 0) return false;

	// 喂狗
	Sys.Sleep(1);

	// 当检测到	     高电平结束  就是下降沿的到来
	retry = 200;
	while (Port == true && retry-- > 0) Sys.Delay(100);
	if (retry <= 0) return false;

	// 计算10ms为基数的当前延迟
	int ms = (int)Sys.Ms() / 10;
	// 折算为需要等待的时间
	//ms = 10 - ms;
	// 加上对齐纠正时间
	//ms += AdjustTime;
	//ms %= 10;

	debug_printf("ACZero::Check 交流零点延迟 %dms \r\n", ms);

	// 计算加权平均数
	Time = (Time + ms) / 2;

	return true;
}
