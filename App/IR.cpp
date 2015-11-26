#include "IR.h"
#include "Time.h"

IR::IR(PWM* pwm, Pin tx, Pin rx)
{
	_Pwm	= pwm;
	_Tim	= NULL;
	_Arr	= NULL;
	_Index	= 0;
	_Ticks	= 0;

	Opened	= false;

	if(tx != P0) Tx.Set(tx).Open();
	if(rx != P0) Rx.Set(rx).Open();
}

bool IR::Open()
{
	if(Opened) return false;

	// 申请一个定时器
	if(_Tim == NULL) _Tim = Timer::Create();
	// 配置定时器的参数
	if(_Tim == NULL) return false;

	Tx	= true;

	_Tim->SetFrequency(20000);  // 50us
	_Tim->Register(
		[](void* s, void* e){
			auto ir = (IR*)s;
			if(ir->_Mode)
				ir->OnSend();
			else
				ir->OnReceive();
		}
		, this);

	Opened	= true;

	return true;
}

bool IR::Close()
{
	if(!Opened) return false;

	if(_Tim != NULL) _Tim->Stop();

	Opened	= false;

	return true;
}

bool IR::Send(const Array& bs)
{
	if(bs.Length() < 2) return false;
	if(!Open()) return false;

	_Arr	= (Array*)&bs;
	_Index	= 0;
	_Ticks	= bs[0];
	_Mode	= true;	// 发送模式，完成时为false

	_Pwm->Start();
	_Tim->Start();
	// 放到后面来 定时器启动代码执行时间造成的影响明显变小
	Tx	= false;

	// 等待发送完成
	TimeWheel tw(1);
	while(_Mode && !tw.Expired());

	_Tim->Stop();
	_Pwm->Stop();
	Tx	= false;

	return true;
}

void IR::OnSend()
{
	// 当前小周期数是否发完
	if(_Ticks-- > 0) return;

	// 倒转
	Tx	= !Tx;

	if(++_Index >= _Arr->Length())
	{
		_Pwm->Stop();
		_Tim->Stop();

		_Mode	= false;
	}
	else
		// 使用下一个元素
		_Ticks = (*_Arr)[_Index];
}

int IR::Receive(Array& bs)
{
	if(!Open()) return false;

	bs.SetLength(0);
	_Arr	= &bs;
	_Index	= -1;
	_Mode	= false;	// 接收模式，完成时为true

	_Tim->Start();

	// 等待发送完成
	TimeWheel tw(1);
	while(!_Mode && !tw.Expired());

	return bs.Length();
}

void IR::OnReceive()
{
	// 获取引脚状态
	bool val = Rx.Read();

	// 等待接收开始状态
	if(_Index == -1)
	{
		if(val) return;

		_Index	= 0;
		_Ticks	= 0;
		_Last	= val;
	}

	// 接收
	_Ticks++;
	if(val != _Last)
	{
		// 产生跳变，记录当前小周期数
		_Arr->SetItemAt(_Index++, &_Ticks);

		_Ticks	= 0;
		_Last	= val;

		// 大于500个跳变沿 表示出错
		if(_Index > 500)
		{
			_Tim ->Stop();
			_Mode	= true;
		}
	}
}
