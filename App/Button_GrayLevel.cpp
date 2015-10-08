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
byte Button_GrayLevel::OnGrayLevel	= 0xff;			// 开灯时 led 灰度
byte Button_GrayLevel::OffGrayLevel	= 0x00;			// 关灯时 led 灰度
int Button_GrayLevel::ACZeroAdjTime=2300;

Button_GrayLevel::Button_GrayLevel()
{
#if DEBUG
	Name	= NULL;
#endif
	Index	= 0;
	_Value	= false;

	_GrayLevelDrive = NULL;
	_PulseIndex = 0xff;

	_Handler	= NULL;
	_Param		= NULL;

	_tid	= 0;
	Next	= 0xFF;
}

Button_GrayLevel::~Button_GrayLevel()
{
}

void Button_GrayLevel::Set(Pin key, Pin relay)
{
	Set(key, relay, true);
}

void Button_GrayLevel::Set(Pin key, Pin relay, bool relayInvert)
{
	assert_param(key != P0);

	// 中断过滤模式，0x01表示使用按下，0x02表示使用弹起
	Key.Mode = 0x02;
	Key.Set(key);
	Key.Register(OnPress, this);
	Key.Open();

	if(relay != P0)
	{
		Relay.Invert = relayInvert;
		Relay.Set(relay).Open();
	}
}

void Button_GrayLevel::Set(PWM* drive, byte pulseIndex)
{
	if(drive && pulseIndex < 4)
	{
		_GrayLevelDrive = drive;
		_PulseIndex = pulseIndex;
		// 刷新输出
		RenewGrayLevel();
	}
}

void Button_GrayLevel::RenewGrayLevel()
{
	if(_GrayLevelDrive)
	{
		_GrayLevelDrive->Pulse[_PulseIndex] = _Value? (0xff-OnGrayLevel) : (0xff-OffGrayLevel);
		_GrayLevelDrive->Config();
	}
}

void Button_GrayLevel::OnPress(Pin pin, bool down, void* param)
{
	Button_GrayLevel* btn = (Button_GrayLevel*)param;
	if(btn) btn->OnPress(pin, down);
}

void Button_GrayLevel::OnPress(Pin pin, bool down)
{
	// 每次按下弹起，都取反状态
	if(!down)
	{
		SetValue(!_Value);
		if(_Handler) _Handler(this, _Param);
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
	while(*port == false && retry-- > 0) Sys.Delay(100);	// 检测下降沿   先去掉低电平  while（io==false）
	if(retry <= 0) return false;

	retry = 200;
	while(*port == true && retry-- > 0) Sys.Delay(100);		// 当检测到	     高电平结束  就是下降沿的到来
	if(retry <= 0) return false;

	return true;
}

void Button_GrayLevel::SetValue(bool value)
{
	if(ACZero)
	{
		//int time = ACZeroAdjTime;
		if(CheckZero(ACZero)) Sys.Delay(ACZeroAdjTime);
		//Sys.Dlay() 参数>=1000 就会切换任务  中断里面不允许
		/*while(time > 700)
		{
			Sys.Delay(700);
			time-=700;
		}
		Sys.Delay(time);*/
		// 经检测 过零检测电路的信号是  高电平12ms  低电平7ms    即下降沿后8.5ms 是下一个过零点
		// 从给出信号到继电器吸合 测量得到的时间是 6.4ms  继电器抖动 1ms左右  即  平均在7ms上下
		// 故这里添加1ms延时
		// 这里有个不是问题的问题   一旦过零检测电路烧了   开关将不能正常工作
	}

	Relay = value;

	_Value = value;

	RenewGrayLevel();
}

bool Button_GrayLevel::SetACZeroPin(Pin aczero)
{
	// 检查参数
	assert_param(aczero != P0);

	// 该方法可能被初级工程师多次调用，需要检查并释放旧的，避免内存泄漏
	if(ACZero) delete ACZero;

	ACZero = new InputPort(aczero);

	// 需要检测是否有交流电，否则关闭
	if(CheckZero(ACZero)) return true;

	delete ACZero;
	ACZero = NULL;

	return false;
}
