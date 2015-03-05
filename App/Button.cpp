#include "Button.h"


InputPort* Button::ACZero = NULL;
int Button::ACZeroAdjTime=2300;

void Button::Init()
{
	Key = NULL;
	Led = NULL;
	Relay = NULL;

	Name = NULL;
	Index = 0;
	_Value = false;
	_Handler = NULL;
	_Param = NULL;
}

Button::Button(Pin key, Pin led, Pin relay)
{
	Init();

	assert_param(key != P0);
	Key = new InputPort(key);
	Key->Register(OnPress, this);

	if(led != P0) Led = new OutputPort(led);
	if(relay != P0) Relay = new OutputPort(relay);
}

Button::Button(Pin key, Pin led, bool ledInvert, Pin relay, bool relayInvert)
{
	Init();

	assert_param(key != P0);
	Key = new InputPort(key);
	Key->Register(OnPress, this);

	if(led != P0) Led = new OutputPort(led, ledInvert);
	if(relay != P0) Relay = new OutputPort(relay, relayInvert);

	//SetValue(false);
}

Button::~Button()
{
	if(Key) delete Key;
	Key = NULL;

	if(Led) delete Led;
	Led = NULL;

	if(Relay) delete Relay;
	Relay = NULL;
}

void Button::OnPress(Pin pin, bool down, void* param)
{
	Button* btn = (Button*)param;
	if(btn) btn->OnPress(pin, down);
}

void Button::OnPress(Pin pin, bool down)
{
	// 每次按下弹起，都取反状态
	if(!down)
	{
		SetValue(!_Value);

		if(_Handler) _Handler(this, _Param);
	}
}

void Button::Register(EventHandler handler, void* param)
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

bool Button::GetValue() { return _Value; }

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

void Button::SetValue(bool value)
{
	if(ACZero)
	{
		if(!CheckZero(ACZero)) return;
		Sys.Delay(ACZeroAdjTime);	// 经检测 过零检测电路的信号是  高电平12ms  低电平7ms    即下降沿后8.5ms 是下一个过零点  
									// 从给出信号到继电器吸合 测量得到的时间是 6.4ms  继电器抖动 1ms左右  即  平均在7ms上下  
									// 故这里添加1ms延时
									// 这里有个不是问题的问题   一旦过零检测电路烧了   开关将不能正常工作
	}
	if(Led) *Led = value;
#ifdef Door_lock
	if(Relay) *Relay = !value;
#else
	if(Relay) *Relay = value;
#endif
	_Value = value;
}

bool Button::SetACZeroPin(Pin aczero)
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
