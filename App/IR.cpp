
#include "IR.h"

IR::IR(PWM * pwm ,Pin Outio,Pin Inio)
{
	_irf = NULL;
	_Outio= NULL;

	_stat = Over;
	_mode = idle;

	_timer = NULL;
	_timerTick = 0x00000;
	_buff = NULL;
	_length = 0;

	if(pwm)_irf = pwm;
	if(Outio!=P0) _Outio = new OutputPort(Outio,true);
	if(Inio != P0) _Inio = new InputPort(Inio);
	SetIRL();
}

bool IR::SetMode(IRMode mode)
{
	if(_mode == mode)return true;
	if(mode != idle)
	{
		// 申请一个定时器
		if(_timer==NULL)
			_timer = Timer::Create();
		// 配置定时器的参数
		if(_timer==NULL)return false;
		_timer->SetFrequency(20000);  // 50us
		_timer->Register(_timerHandler,this);

		_mode = mode;

		if(mode == receive)	_stat = WaitRec;
		else 				_stat = WaitSend;
	}
	else
	{
		if(_timer!=NULL)
		{
			_timer->Stop();
			_timer->Register((EventHandler)NULL,NULL);
//			delete(_timer);		// 不一定要释放掉
			_stat=Over;
			_mode = mode;
		}
	}
	return true;
}

bool IR::Send(byte *sendbuf,int length)
{
	if(!_Outio)return false;
	if(!_irf)return false;
	if(length<2)return false;
	if(*sendbuf != (byte)length)return false;
	
	// Tick清零
	_timerTick=0x00000;
	
	// 去掉头字节长度
	_buff = sendbuf;
	_length = length;
	
	// 模式切换
	if(_mode != idle)return false;
	if(!SetMode(send))return false;

	_nextOut = false;
	_irf->Start();
	//挪到后面一点	SetIRH();
	// 这一句执行时间比较长   影响第一个波形 怎么整
	_timer->Start();	
	// 放到后面来 定时器启动代码执行时间造成的影响明显变小
	*_Outio=false;
	// 更新状态
	_stat = Sending;

	// 等待发送完成
	for(int i=0;i<2;i++)
	{
		// 等500ms
		Sys.Sleep(500);  
		if(_stat == Over)break;
	}
	// 如果还在发送中 再等他200ms
	if(_stat == Sending)
		Sys.Sleep(200);  
	// 还在发送 表示出错
	if(_stat == Sending)
	{
		_stat = SendError;	
		_timer->Stop();
	}
	
	*_Outio=false;
	_irf->Stop();
	// 放开模式
	if(!SetMode(idle))return false;
	
	return true;
}

int IR::Receive(byte *buff)
{
	if(!_Inio)return -2;
	if(_mode != idle)return 0;
	SetMode(receive);
	_timerTick = 0x0000;

	_buff = buff;
	_length = 0;
	_timer->Start();
	// 等待5s  如果没有信号 判断为超时
	for(int i=0;i<10;i++)
	{
		Sys.Sleep(500);  // 等500ms
		if(_stat == Over)break;
	}
	
	if(_stat == Recing)
		Sys.Sleep(500);  // 如果还在接受中 再等他500ms
		
	if(_stat == Recing)
		_stat = RecError;	// 还在接收 表示出错

	SetMode(idle);

	if(_stat == RecError) return -2;

	buff[0] = _length;					// 第一个字节放长度
	if(_length == 0)return -1;
	if(_length <=5)return -2;			// 数据太短 也表示 接收出错
	return _length;
}

// 静态中断部分
void  IR::_timerHandler(void* sender, void* param)
{
	IR* ir = (IR*)param;
	ir->SendReceHandler(sender,NULL);
}

// 真正的中断函数  红外的主要部分
const bool ioIdleStat = true;

void IR::SendReceHandler(void* sender, void* param)
{
	volatile static bool ioOldStat;
	
	switch(_mode)
	{
		case receive:
		{
			// 获取引脚状态
			bool s = _Inio->Read();
			
			// 等待接收开始状态
			if(_stat == WaitRec)
			{
				if(s != ioIdleStat)
				{
				// 等待到第一次状态
					_stat = Recing;
					_timerTick = 0x0000;
					ioOldStat = s;
					// 留空第一个字节
					_buff++;
					_length ++;
				}
				else
				{
					return;
				}
			}
			
			// 接收
			_timerTick++;
			if(s != ioOldStat)
			{
				ioOldStat = s;
				
				*_buff++ = (byte)_timerTick;
				
				_timerTick = 0x0000;
				_length ++;
				
				// 大于500个跳变沿 表示出错
				if(_length>500)	
				{
					_timer ->Stop();
					_stat = RecError;
				}
			}
			break;
		}

		case send:
		{
			_timerTick++;
			if(*_buff == _timerTick)
			{
				if(_nextOut)	/*SetIRH();*/*_Outio=false;
				else 			/*SetIRL();*/*_Outio=true;

				_nextOut = !_nextOut;

				_timerTick = 0x0000;
				_buff ++;
				_length --;
				if(_length == 0)
				{
					_timerTick=0x0000;
					_irf->Stop();
					_timer ->Stop();
					_stat = Over;
				}
			}
			else 
				return;
		}break;
		
		default:_mode = idle;	break ;
	}

	// 不允许 超过 25.5ms 的东西  不论发送还是接收
	// 空闲模式直接 关闭定时器
	if(_timerTick > 1000 || _mode == idle)
	{
		_timerTick=0x0000;
		_timer ->Stop();
		if(_stat == Recing)
		{
			_stat = Over;		// 接收时超时就表示接收结束
		}
	}
}

