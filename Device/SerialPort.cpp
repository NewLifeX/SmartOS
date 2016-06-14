#include "Sys.h"
#include "Task.h"

#include "SerialPort.h"
#include "Time.h"

#define COM_DEBUG 0

SerialPort::SerialPort() { Init(); }

SerialPort::SerialPort(COM index, int baudRate)
{
	Init();
	Set(index, baudRate);
}

// 析构时自动关闭
SerialPort::~SerialPort()
{
	if(RS485) delete RS485;
	RS485 = nullptr;
}

void SerialPort::Init()
{
	_index	= 0xFF;
	RS485	= nullptr;
	Error	= 0;

	Remap	= false;
	MinSize	= 1;

	_taskidRx	= 0;

	OnInit();
}

void SerialPort::Set(COM index, int baudRate)
{
	_index		= index;
    _baudRate	= baudRate;

	// 计算字节间隔。字节速度一般是波特率转为字节后再除以2
	//ByteTime = 15000000 / baudRate;  // (1000000 /(baudRate/10)) * 1.5
	//ByteTime	= 1000000 / (baudRate >> 3 >> 1);
	//ByteTime	<<= 1;
	if(baudRate > 9600)
		ByteTime	= 1;
	else
		ByteTime	= 1000 / (baudRate / 10) + 1;	// 小数部分忽略，直接加一

	// 设置名称
	Buffer(Name, 4)	= "COMx";
	Name[3] = '0' + _index + 1;
	Name[4] = 0;

	// 根据端口实际情况决定打开状态
	if(OnSet()) Opened = true;
}

void SerialPort::Set(byte parity, byte dataBits, byte stopBits)
{
    _parity		= parity;
    _dataBits	= dataBits;
    _stopBits	= stopBits;
}

// 打开串口
bool SerialPort::OnOpen()
{
    debug_printf("Serial%d Open(%d, %d, %d, %d)\r\n", _index + 1, _baudRate, _parity, _dataBits, _stopBits);

	// 清空缓冲区
//#if !(defined(STM32F0) || defined(GD32F150))
	Tx.Clear();
//#endif
	Rx.SetCapacity(0x80);
	Rx.Clear();

	OnOpen2();

	if(RS485) *RS485 = false;

	return true;
}

// 关闭端口
void SerialPort::OnClose()
{
    debug_printf("~Serial%d Close\r\n", _index + 1);

	OnClose2();
}

// 向某个端口写入数据。如果size为0，则把data当作字符串，一直发送直到遇到\0为止
bool SerialPort::OnWrite(const Buffer& bs)
{
	if(!bs.Length()) return true;
/*#if defined(STM32F0) || defined(GD32F150)
	if(RS485) *RS485 = true;
	// 中断发送过于频繁，影响了接收中断，采用循环阻塞发送。后面考虑独立发送任务
	for(int i=0; i<bs.Length(); i++)
	{
		SendData(bs[i], 3000);
	}
	if(RS485) *RS485 = false;
#else*/
	// 如果队列已满，则强制刷出
	if(Tx.Length() + bs.Length() > Tx.Capacity()) Flush(Sys.Clock / 40000);

	Tx.Write(bs);

	// 打开串口发送
	if(RS485) *RS485 = true;
	//USART_ITConfig((USART_TypeDef*)_port, USART_IT_TXE, ENABLE);
	OnWrite2();
//#endif

	return true;
}

// 刷出某个端口中的数据
bool SerialPort::Flush(uint times)
{
//#if !(defined(STM32F0) || defined(GD32F150))
	// 打开串口发送
	if(RS485) *RS485 = true;

	while(!Tx.Empty() && times > 0) times = SendData(Tx.Pop(), times);

	if(RS485) *RS485 = false;

	return times > 0;
/*#else
	return true;
#endif*/
}

// 从某个端口读取数据
uint SerialPort::OnRead(Buffer& bs)
{
	uint count = 0;
	uint len = Rx.Length();
	// 如果没有数据，立刻返回，不要等待浪费时间
	if(!len)
	{
		bs.SetLength(0);

		return 0;
	}

	// 如果有数据变化，等一会
	while(len != count && len < bs.Length())
	{
		count = len;
		// 按照115200波特率计算，传输7200字节每秒，每个毫秒7个字节，大概150微秒差不多可以接收一个新字节
		Sys.Sleep(ByteTime);
		//Sys.Sleep(2);
		len = Rx.Length();
	}
	// 如果数据大小不足，等下次吧
	if(len < MinSize)
	{
		bs.SetLength(0);

		return 0;
	}

	// 从接收队列读取
	count = Rx.Read(bs);
	bs.SetLength(count);

	// 如果还有数据，打开任务
	if(!Rx.Empty()) Sys.SetTask(_taskidRx, true, 0);

	return count;
}

void SerialPort::ReceiveTask()
{
	//auto sp = (SerialPort*)param;
	auto sp	= this;

	//!!! 只要注释这一行，四位触摸开关就不会有串口溢出错误
	if(sp->Rx.Length() == 0) return;

	// 从栈分配，节省内存
	byte buf[0x100];
	Buffer bs(buf, ArrayLength(buf));
	int mx	= sp->MaxSize;
	if(mx > 0 && mx > bs.Length()) bs.SetLength(mx);

	uint len = sp->Read(bs);
	if(len)
	{
		len = sp->OnReceive(bs, nullptr);
		// 如果有数据，则反馈回去
		if(len) sp->Write(bs);
	}
}

void SerialPort::SetBaudRate(int baudRate)
{
	Set((COM)_index,  baudRate);
}

void SerialPort::ChangePower(int level)
{
	// 串口进入低功耗时，直接关闭
	if(level) Close();
}

void SerialPort::Register(TransportHandler handler, void* param)
{
	ITransport::Register(handler, param);

    if(handler)
	{
		// 建立一个未启用的任务，用于定时触发接收数据，收到数据时开启
		if(!_taskidRx)
		{
			//_taskidRx = Sys.AddTask([](void* p){ ((SerialPort*)p)->ReceiveTask(); }, this, -1, -1, "串口接收");
			_taskidRx = Sys.AddTask(&SerialPort::ReceiveTask, this, -1, -1, "串口接收");
			auto tsk = Task::Get(_taskidRx);
			// 串口任务深度设为2，允许重入，解决接收任务内部调用发送然后又等待接收匹配的问题
			tsk->MaxDeepth	= 2;
			_task	= tsk;
		}
/*#if defined(STM32F0) || defined(GD32F150)
		// 打开中断
		byte irq = uart_irqs[_index];
		Interrupt.SetPriority(irq, 0);
		Interrupt.Activate(irq, OnHandler, this);
#endif*/
	}
    else
	{
		Sys.RemoveTask(_taskidRx);
	}
}

extern "C"
{
    SerialPort* _printf_sp;
}

SerialPort* SerialPort::GetMessagePort()
{
	auto sp	= _printf_sp;
	// 支持中途改变调试口
	if(sp && Sys.MessagePort != sp->_index)
	{
		delete sp;
		_printf_sp	= nullptr;
	}

	if(!sp)
	{
        auto idx	= (COM)Sys.MessagePort;
        if(idx == COM_NONE) return nullptr;

		sp = _printf_sp = new SerialPort(idx);
		sp->Open();
	}
	return sp;
}
