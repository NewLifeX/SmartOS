#include "Button_GrayLevel.h"


InputPort* Button_GrayLevel::ACZero = NULL;
int Button_GrayLevel::ACZeroAdjTime=2300;

Button_GrayLevel::Button_GrayLevel()
{
	Name	= NULL;
	Index	= 0;
	_Value	= false;
	
	_GrayLevelDrive = NULL;
	_PulseIndex = 0xff;

	_Handler	= NULL;
	_Param		= NULL;
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

	Key.Set(key).Open();
	Key.Register(OnPress, this);

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
		_GrayLevelDrive->Pulse[_PulseIndex] = _Value? OnGrayLevel:OffGrayLevel;
		_GrayLevelDrive->Start();
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
		if(!CheckZero(ACZero)) return;
		Sys.Delay(ACZeroAdjTime);	// 经检测 过零检测电路的信号是  高电平12ms  低电平7ms    即下降沿后8.5ms 是下一个过零点
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
