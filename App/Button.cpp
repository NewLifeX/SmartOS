#include "Button.h"

InputPort Button::ACZero;
int Button::ACZeroAdjTime = 2300;

Button::Button()
{
	Name	= nullptr;
	Index	= 0;
	_Value	= false;

	//_Handler	= nullptr;
	//_Param		= nullptr;
}

Button::~Button()
{
}

void Button::Set(Pin key, Pin led, Pin relay)
{
	Set(key, led, true, relay, true);
}

void Button::Set(Pin key, Pin led, bool ledInvert, Pin relay, bool relayInvert)
{
	Key.Set(key);
	//Key.Register(OnPress, this);
	//Key.Press	= OnPress;
	Key.Press.Bind(&Button::OnPress, this);
	Key.UsePress();
	//Key.Register(Delegate2<InputPort&, bool>(&Button::OnPress, this))
	Key.Open();

	if(led != P0)
	{
		Led.Invert = ledInvert;
		Led.Set(led).Open();
	}
	if(relay != P0)
	{
		Relay.Invert = relayInvert;
		Relay.Set(relay).Open();
	}
}

/*void Button::OnPress(InputPort* port, bool down, void* param)
{
	Button* btn = (Button*)param;
	if(btn) btn->OnPress(port->_Pin, down);
}*/

void Button::OnPress(InputPort& port, bool down)
{
	// 每次按下弹起，都取反状态
	if(!down)
	{
		SetValue(!_Value);

		//if(_Handler) _Handler(this, _Param);
		Press(*this);
	}
}

/*void Button::Register(EventHandler handler, void* param)
{
	if(handler)
	{
		_Handler = handler;
		_Param = param;
	}
	else
	{
		_Handler = nullptr;
		_Param = nullptr;
	}
}*/

bool Button::GetValue() { return _Value; }

int Button::OnWrite(byte data)
{
	SetValue(data);

	return OnRead();
}

byte Button::OnRead()
{
	return _Value ? 1 : 0;
}

String& Button::ToStr(String& str) const
{
	if(Name)
		str += Name;
	else
		Object::ToStr(str);

	return str;
}

bool CheckZero(const InputPort& port)
{
	int retry = 200;
	while(!port.Read() && retry-- > 0) Sys.Delay(100);	// 检测下降沿   先去掉低电平  while（io==false）
	if(retry <= 0) return false;

	retry = 200;
	while(port.Read() && retry-- > 0) Sys.Delay(100);		// 当检测到	     高电平结束  就是下降沿的到来
	if(retry <= 0) return false;

	return true;
}

void Button::SetValue(bool value)
{
	if(!ACZero.Empty())
	{
		if(CheckZero(ACZero)) Sys.Delay(ACZeroAdjTime);

		// 经检测 过零检测电路的信号是  高电平12ms  低电平7ms    即下降沿后8.5ms 是下一个过零点
		// 从给出信号到继电器吸合 测量得到的时间是 6.4ms  继电器抖动 1ms左右  即  平均在7ms上下
		// 故这里添加1ms延时
		// 这里有个不是问题的问题   一旦过零检测电路烧了   开关将不能正常工作
	}
	Led		= value;
	Relay	= value;

	_Value	= value;
}

bool Button::SetACZeroPin(Pin aczero)
{
	auto& port = ACZero;

	// 该方法可能被初级工程师多次调用，需要检查并释放旧的，避免内存泄漏
	if(!port.Empty()) port.Close();

	port.Set(aczero).Open();

	// 需要检测是否有交流电，否则关闭
	if(CheckZero(port)) return true;

	port.Close();
	port.Clear();

	return false;
}
