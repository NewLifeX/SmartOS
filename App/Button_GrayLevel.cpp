#include "Kernel\Sys.h"
#include "Kernel\Task.h"
#include "Device\Port.h"

#include "Device\Timer.h"
#include "Button_GrayLevel.h"
#include "Device\WatchDog.h"

//#include "Platform\stm32.h"

#define BTN_DEBUG DEBUG
//#define BTN_DEBUG 0
#if BTN_DEBUG
#define btn_printf debug_printf
#else
#define btn_printf(format, ...)
#endif

Button_GrayLevelConfig*	ButtonConfig = nullptr;

/******************************** 调光配置 ********************************/
Button_GrayLevelConfig::Button_GrayLevelConfig()
{
	_Name = "Gray";
	_Start = &OnGrayLevel;
	_End = &TagEnd;
	Init();

	OnGrayLevel = 0xff;
	OffGrayLevel = 20;
}

InputPort* Button_GrayLevel::ACZero = nullptr;
int Button_GrayLevel::ACZeroAdjTime = 2300;

Button_GrayLevel::Button_GrayLevel() : ByteDataPort()
{
#if DEBUG
	Name = nullptr;
#endif
	Index = 0;
	_Value = false;

	_Pwm = nullptr;
	_Channel = 0;

	//_Handler = nullptr;
	//_Param = nullptr;
	EnableDelayClose = true;
	_tid = 0;
	Next = 0xFF;
}

void Button_GrayLevel::Set(Pin key, Pin relay, bool relayInvert)
{
	Key.Set(key);

	// 中断过滤模式
	//Key.Mode = InputPort::Both;

	Key.ShakeTime = 20;
	//Key.Register(OnPress, this);
	Key.Press.Bind(&Button_GrayLevel::OnPress, this);
	Key.HardEvent = true;

	Key.UsePress();
	Key.Open();

	//面板事件
	if (!_task3)
	{
		_task3 = Sys.AddTask(&Button_GrayLevel::ReportPress, this, -1, -1, "面板事件");
	}
	if (relay != P0) Relay.Init(relay, relayInvert).Open();
}

void Button_GrayLevel::Set(Pwm* drive, byte pulseIndex)
{
	if (drive && pulseIndex < 4)
	{
		_Pwm = drive;
		_Channel = pulseIndex;

		// 刷新输出
		RenewGrayLevel();
	}
}

void Button_GrayLevel::RenewGrayLevel()
{
	auto on = ButtonConfig->OnGrayLevel;
	auto off = ButtonConfig->OffGrayLevel;

	if (_Pwm)
	{
		_Pwm->Pulse[_Channel] = _Value ? (0xFF - on) : (0xFF - off);
		_Pwm->Flush();
	}
}

void Button_GrayLevel::GrayLevelUp()
{
	auto on = ButtonConfig->OnGrayLevel;
	if (_Pwm)
	{
		_Pwm->Pulse[_Channel] = (0xFF - on);
		_Pwm->Flush();
	}
}

void Button_GrayLevel::GrayLevelDown()
{
	auto off = ButtonConfig->OffGrayLevel;
	if (_Pwm)
	{
		_Pwm->Pulse[_Channel] = (0xFF - off);
		_Pwm->Flush();
	}
}

/*void Button_GrayLevel::OnPress(InputPort* port, bool down, void* param)
{
	Button_GrayLevel* btn = (Button_GrayLevel*)param;
	if (btn) btn->OnPress(port, down);
}*/

void Button_GrayLevel::OnPress(InputPort& port, bool down)
{
	//只有弹起才能执行业务动作
	if (down) return;

	ushort time = port.PressTime;

	debug_printf("按键时长 %d \r\n", time);
	if (_Value && EnableDelayClose)
	{
		if (time > 1500)
		{
			debug_printf("DelayClose  ");
			if (time >= 3800 && time < 6500)
			{
				debug_printf("60s\r\n");
				DelayClose2(60 * 1000);
				port.PressTime = 0;	// 保险一下，以免在延时关闭更新状态的时候误判造成重启
				return;
			}
			if (time < 3800)
			{
				debug_printf("15s\r\n");
				DelayClose2(15 * 1000);
				port.PressTime = 0;	// 保险一下，以免在延时关闭更新状态的时候误判造成重启
				return;
			}
		}
	}

	SetValue(!_Value);

	//取消延迟关闭
	if (_task2)
	{
		Sys.RemoveTask(_task2);
		_task2 = 0;
	}
	// if (_Handler) _Handler(this, _Param);
	if (_task3)
		Sys.SetTask(_task3, true, 0);
	else
		Press(*this);
}

void Button_GrayLevel::ReportPress()
{
	Press(*this);
}

void Close2Task(void * param)
{
	auto bt = (Button_GrayLevel *)param;
	bt->GrayLevelUp();
	if (bt->delaytime > 1000)
	{
		bt->delaytime = bt->delaytime - 1000;
		Sys.SetTask(bt->_task2, true, 1000);
	}
	else
	{
		bt->delaytime = 0;
		debug_printf("延时关闭已完成 删除任务 %p\r\n", bt->_task2);
		Sys.RemoveTask(bt->_task2);
		bt->_task2 = 0;
		bt->SetValue(false);
		bt->Key.PressTime = 0;
		bt->Press(*bt);			// 维护数据区状态
	}
	Sys.Sleep(100);
	bt->GrayLevelDown();
}

void Button_GrayLevel::DelayClose2(int ms)
{
	if (!_task2 && ms)
	{
		_task2 = Sys.AddTask(Close2Task, this, 0, 1000, "延时关闭");
	}

	delaytime = ms;
}

int Button_GrayLevel::OnWrite(byte data)
{
	SetValue(data > 0);

	return OnRead();
}

byte Button_GrayLevel::OnRead()
{
	return _Value ? 1 : 0;
}

bool Button_GrayLevel::GetValue() { return _Value; }

bool CheckZero(InputPort* port)
{
	// 过零检测代码有风险  强制执行喂狗任务确保不出问题
	//auto dog	= Task::Scheduler()->FindTask(WatchDog::FeedDogTask);
	//if(dog) dog->Execute(Sys.Ms());

	// 检测下降沿   先去掉低电平  while（io==false）
	int retry = 200;
	while (*port == false && retry-- > 0) Sys.Delay(100);
	if (retry <= 0) return false;

	// 喂狗
	Sys.Sleep(1);

	// 当检测到	     高电平结束  就是下降沿的到来
	retry = 200;
	while (*port == true && retry-- > 0) Sys.Delay(100);
	if (retry <= 0) return false;

	return true;
}

void Button_GrayLevel::SetValue(bool value)
{
	_Value = value;

	if (ACZero && ACZero->Opened)
	{
		if (CheckZero(ACZero)) Sys.Delay(ACZeroAdjTime);
		// 经检测 过零检测电路的信号是  高电平12ms  低电平7ms    即下降沿后8.5ms 是下一个过零点
		// 从给出信号到继电器吸合 测量得到的时间是 6.4ms  继电器抖动 1ms左右  即  平均在7ms上下
		// 故这里添加1ms延时
		// 这里有个不是问题的问题   一旦过零检测电路烧了   开关将不能正常工作
	}

	Relay = value;

	RenewGrayLevel();
}

bool Button_GrayLevel::SetACZeroPin(Pin aczero)
{
	// 该方法需要检查并释放旧的，避免内存泄漏
	if (!ACZero) ACZero = new InputPort(aczero);

	// 需要检测是否有交流电，否则关闭
	if (CheckZero(ACZero)) return true;

	ACZero->Close();

	return false;
}

void Button_GrayLevel::Init(TIMER tim, byte count, Button_GrayLevel* btns, TAction onpress, const ButtonPin* pins, byte* level, const byte* state)
{
	debug_printf("\r\n初始化开关按钮 \r\n");

	// 配置PWM来源
	static Pwm pwm(tim);
	// 设置分频 尽量不要改 Prescaler * Period 就是 Pwm 周期
	pwm.Prescaler = 0x04;		// 随便改  只要肉眼看不到都没问题
	pwm.Period = 0xFF;		// 对应灰度调节范围
	pwm.Polarity = true;		// 极性。默认true高电平。如有必要，将来根据Led引脚自动检测初始状态

	// 配置 LED 引脚
	static AlternatePort Leds[4];

	/*pwm.Remap	= GPIO_PartialRemap_TIM3;
	for (int i = 0; i < count; i++)
	{
		Leds[i].Set(pins[i].Led);
		//Leds[i].Open();
		Leds[i].AFConfig(Port::AF_1);

		pwm.Ports[i]	= new AlternatePort(Leds[i]);
		pwm.Enabled[i]	= true;
	}*/

	pwm.Open();

	// 设置默认灰度
	if (level[0] == 0x00)
	{
		level[0] = 250;
		level[1] = 20;
	}

	// 配置 Button 主体
	for (int i = 0; i < count; i++)
	{
		btns[i].Set(pins[i].Key, pins[i].Relay, pins[i].Invert);
	}

#if DEBUG
	cstring names[] = { "一号", "二号", "三号", "四号" };
#endif

	for (int i = 0; i < count; i++)
	{
		btns[i].Index = i;
#if DEBUG
		btns[i].Name = names[i];
#endif
		//btns[i].Register(onpress);
		btns[i].Press = onpress;

		// 灰度 LED 绑定到 Button
		btns[i].Set(&pwm, pins[i].PwmIndex);

		// 如果是热启动，恢复开关状态数据
		if (state && state[i]) btns[i].SetValue(true);
	}
}

void ACZeroReset(void *param)
{
	InputPort* port = Button_GrayLevel::ACZero;
	if (port)
	{
		//Sys.Reboot();
		debug_printf("定时检查过零检测\r\n");

		// 需要检测是否有交流电，否则关闭
		if (CheckZero(port)) return;

		port->Close();
	}
}

void Button_GrayLevel::InitZero(Pin zero, int us)
{
	if (zero == P0) return;

	debug_printf("\r\n过零检测引脚PB12探测\r\n");
	Button_GrayLevel::SetACZeroAdjTime(us);

	if (Button_GrayLevel::SetACZeroPin(zero))
		debug_printf("已连接交流电！\r\n");
	else
		debug_printf("未连接交流电或没有使用过零检测电路\r\n");

	debug_printf("所有开关按钮准备就绪！\r\n\r\n");

	Sys.AddTask(ACZeroReset, nullptr, 60000, 60000, "定时过零");
}

Button_GrayLevelConfig Button_GrayLevel::InitGrayConfig()
{
	static Button_GrayLevelConfig	cfg;
	if (!ButtonConfig)
	{
		ButtonConfig = &cfg;
		ButtonConfig->Load();

		debug_printf("开关灰度%d,%d\r\n", cfg.OnGrayLevel, cfg.OffGrayLevel);
	}
	return cfg;
}

void Button_GrayLevel::UpdateLevel(byte* level, Button_GrayLevel* btns, byte count)
{
	ButtonConfig->OnGrayLevel = level[0];
	ButtonConfig->OffGrayLevel = level[1];
	ButtonConfig->Save();

	debug_printf("指示灯灰度调整\r\n");
	for (int i = 0; i < count; i++)
		btns[i].RenewGrayLevel();
}
