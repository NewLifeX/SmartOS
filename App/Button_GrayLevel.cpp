#include "Button_GrayLevel.h"
#include "Time.h"

#define BTN_DEBUG DEBUG
//#define BTN_DEBUG 0
#if BTN_DEBUG
	#define btn_printf debug_printf
#else
	#define btn_printf(format, ...)
#endif

InputPort* Button_GrayLevel::ACZero = NULL;
byte Button_GrayLevel::OnGrayLevel	= 0xFF;			// 开灯时 led 灰度
byte Button_GrayLevel::OffGrayLevel	= 0x00;			// 关灯时 led 灰度
int Button_GrayLevel::ACZeroAdjTime=2300;

Button_GrayLevel::Button_GrayLevel() : ByteDataPort()
{
#if DEBUG
	Name	= NULL;
#endif
	Index	= 0;
	_Value	= false;

	_Pwm		= NULL;
	_Channel	= 0;

	_Handler	= NULL;
	_Param		= NULL;

	_tid	= 0;
	Next	= 0xFF;

	OnPress	= NULL;
}

void Button_GrayLevel::Set(Pin key, Pin relay, bool relayInvert)
{
	assert_param(key != P0);

	Key.Set(key);

	// 中断过滤模式
	if(!OnPress)
		Key.Mode	= InputPort::Rising;

	Key.ShakeTime	= 40;
	Key.Register(OnKeyPress, this);
	Key.Open();

	if(relay != P0) Relay.Init(relay, relayInvert).Open();
}

void Button_GrayLevel::Set(PWM* drive, byte pulseIndex)
{
	if(drive && pulseIndex < 4)
	{
		_Pwm		= drive;
		_Channel	= pulseIndex;
		// 刷新输出
		RenewGrayLevel();
	}
}

void Button_GrayLevel::RenewGrayLevel()
{
	if(_Pwm)
	{
		_Pwm->Pulse[_Channel] = _Value? (0xFF - OnGrayLevel) : (0xFF - OffGrayLevel);
		_Pwm->Config();
	}
}

void Button_GrayLevel::OnKeyPress(InputPort* port, bool down, void* param)
{
	Button_GrayLevel* btn = (Button_GrayLevel*)param;
	if(btn) btn->OnKeyPress(port, down);
}

void Button_GrayLevel::OnKeyPress(InputPort* port, bool down)
{
	// 每次按下弹起，都取反状态
	if(down)
	{
		SetValue(!_Value);
		if(_Handler) _Handler(this, _Param);
	}
	else
	{
		if(OnPress) OnPress(port, down, this);
	}
}

void Button_GrayLevel::Register(EventHandler handler, void* param)
{
	if(handler)
	{
		_Handler = handler;
		_Param = param;
	}
	else
	{
		_Handler = NULL;
		_Param = NULL;
	}
}

int Button_GrayLevel::OnWrite(byte data)
{
	SetValue(data);

	return OnRead();
}

byte Button_GrayLevel::OnRead()
{
	return _Value ? 1 : 0;
}

bool Button_GrayLevel::GetValue() { return _Value; }

bool CheckZero(InputPort* port)
{
	int retry = 200;
	while(*port == false && retry-- > 0) Time.Delay(100);	// 检测下降沿   先去掉低电平  while（io==false）
	if(retry <= 0) return false;

	retry = 200;
	while(*port == true && retry-- > 0) Time.Delay(100);		// 当检测到	     高电平结束  就是下降沿的到来
	if(retry <= 0) return false;

	return true;
}

void Button_GrayLevel::SetValue(bool value)
{
	_Value	= value;

	if(ACZero && ACZero->Opened)
	{
		if(CheckZero(ACZero)) Time.Delay(ACZeroAdjTime);
		// 经检测 过零检测电路的信号是  高电平12ms  低电平7ms    即下降沿后8.5ms 是下一个过零点
		// 从给出信号到继电器吸合 测量得到的时间是 6.4ms  继电器抖动 1ms左右  即  平均在7ms上下
		// 故这里添加1ms延时
		// 这里有个不是问题的问题   一旦过零检测电路烧了   开关将不能正常工作
	}

	Relay	= value;

	RenewGrayLevel();
}

bool Button_GrayLevel::SetACZeroPin(Pin aczero)
{
	// 检查参数
	assert_param(aczero != P0);

	// 该方法需要检查并释放旧的，避免内存泄漏
	if(!ACZero) ACZero = new InputPort(aczero);

	// 需要检测是否有交流电，否则关闭
	if(CheckZero(ACZero)) return true;

	ACZero->Close();

	return false;
}

void Button_GrayLevel::Init(byte tim, byte count, Button_GrayLevel* btns, EventHandler onpress
	, const ButtonPin* pins, byte* level, const byte* state)
{
	debug_printf("\r\n初始化开关按钮 \r\n");

	// 配置PWM来源
	static PWM LedPWM(tim);
	// 设置分频 尽量不要改 Prescaler * Period 就是 PWM 周期
	LedPWM.Prescaler	= 0x04;		// 随便改  只要肉眼看不到都没问题
	LedPWM.Period		= 0xFF;		// 对应灰度调节范围
	LedPWM.Polarity		= true;		// 极性。默认true高电平。如有必要，将来根据Led引脚自动检测初始状态
	LedPWM.Start();

	// 配置 LED 引脚
	static AlternatePort Leds[4];

	for(int i = 0; i < count; i++)
	{
		Leds[i].Set(pins[i].Led);
		Leds[i].Open();
#if defined(STM32F0) || defined(STM32F4)
		Leds[i].AFConfig(GPIO_AF_1);
#endif
	}

	// 设置默认灰度
	if(level[0] == 0x00)
	{
		level[0] = 250;
		level[1] = 20;
	}
	// 使用 Data 记录的灰度
	OnGrayLevel 	= level[0];
	OffGrayLevel 	= level[1];

	// 配置 Button 主体
	for(int i = 0; i < count; i++)
	{
		btns[i].Set(pins[i].Key, pins[i].Relay, pins[i].Invert);
	}

#if DEBUG
	const string names[] = {"一号", "二号", "三号", "四号"};
#endif

	for(int i=0; i < count; i++)
	{
		btns[i].Index	= i;
#if DEBUG
		btns[i].Name	= names[i];
#endif
		btns[i].Register(onpress);

		// 灰度 LED 绑定到 Button
		btns[i].Set(&LedPWM, pins[i].PwmIndex);

		// 如果是热启动，恢复开关状态数据
		if(state && state[i]) btns[i].SetValue(true);
	}
}

void ACZeroReset(void *param)
{
	InputPort* port = Button_GrayLevel::ACZero;
	if(port)
	{
		//Sys.Reset();
		debug_printf("定时检查过零检测\r\n");

		// 需要检测是否有交流电，否则关闭
		if(CheckZero(port)) return;

		port->Close();
	}
}

void Button_GrayLevel::InitZero(Pin zero, int us)
{
	if(zero == P0) return;

	debug_printf("\r\n过零检测引脚PB12探测\r\n");
	Button_GrayLevel::SetACZeroAdjTime(us);

	if(Button_GrayLevel::SetACZeroPin(zero))
		debug_printf("已连接交流电！\r\n");
	else
		debug_printf("未连接交流电或没有使用过零检测电路\r\n");

	debug_printf("所有开关按钮准备就绪！\r\n\r\n");

	Sys.AddTask(ACZeroReset, NULL, 60000, 60000, "定时过零");
}

bool Button_GrayLevel::UpdateLevel(byte* level, Button_GrayLevel* btns, byte count)
{
	bool rs = false;
	if(OnGrayLevel != level[0])
	{
		OnGrayLevel = level[0];
		rs = true;
	}
	if(OnGrayLevel != level[1])
	{
		OnGrayLevel = level[1];
		rs = true;
	}
	if(rs)
	{
		debug_printf("指示灯灰度调整\r\n");
		for(int i = 0; i < count; i++)
			btns[i].RenewGrayLevel();
	}
	return rs;
}
