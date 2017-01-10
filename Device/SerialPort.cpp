#include "Sys.h"
#include "Kernel\Task.h"

#include "SerialPort.h"

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
	delete RS485;
	RS485 = nullptr;

	delete Ports[0];
	Ports[0] = nullptr;

	delete Ports[1];
	Ports[1] = nullptr;
}

void SerialPort::Init()
{
	Index	= COM_NONE;
	RS485	= nullptr;
	Error	= 0;
	State	= nullptr;

	Remap	= 0;
	MinSize	= 1;

	Pins[0]		= Pins[1]	= P0;
	Ports[0]	= Ports[1]	= nullptr;

    _dataBits	= 8;
    _parity		= 0;
    _stopBits	= 1;

	_taskidRx	= 0;

	OnInit();
}

void SerialPort::Set(COM index, int baudRate)
{
	Index		= index;
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
	Name[3] = '0' + Index + 1;
	Name[4] = 0;

	// 根据端口实际情况决定打开状态
	if(OnSet()) Opened = true;
}

void SerialPort::Set(byte dataBits, byte parity, byte stopBits)
{
    _dataBits	= dataBits;
    _parity		= parity;
    _stopBits	= stopBits;
}

// 打开串口
bool SerialPort::OnOpen()
{
	// 清空缓冲区
	Tx.SetCapacity(256);
	Tx.Clear();
	Rx.SetCapacity(256);
	Rx.Clear();

    debug_printf("Serial%d::Open(%d, %d, %d, %d) TX=P%c%d RX=P%c%d Cache(TX=%d, RX=%d)\r\n", Index + 1, _baudRate, _dataBits, _parity, _stopBits, _PIN_NAME(Pins[0]), _PIN_NAME(Pins[1]), Tx.Capacity(), Rx.Capacity());

	OnOpen2();

	Set485(false);

	return true;
}

// 关闭端口
void SerialPort::OnClose()
{
    debug_printf("~Serial%d Close\r\n", Index + 1);

	OnClose2();
}

// 向某个端口写入数据。如果size为0，则把data当作字符串，一直发送直到遇到\0为止
bool SerialPort::OnWrite(const Buffer& bs)
{
	if(!bs.Length()) return true;
/*#if defined(STM32F0) || defined(GD32F150)
	Set485(true);
	// 中断发送过于频繁，影响了接收中断，采用循环阻塞发送。后面考虑独立发送任务
	for(int i=0; i<bs.Length(); i++)
	{
		SendData(bs[i], 3000);
	}
	Set485(false);
#else*/
	// 如果队列已满，则强制刷出
	//if(Tx.Length() + bs.Length() > Tx.Capacity()) Flush(Sys.Clock / 40000);

	Tx.Write(bs);

	// 打开串口发送
	Set485(true);
	//USART_ITConfig((USART_TypeDef*)State, USART_IT_TXE, ENABLE);
	OnWrite2();
//#endif

	return true;
}

// 刷出某个端口中的数据
bool SerialPort::Flush(uint times)
{
	// 打开串口发送
	Set485(true);

	while(!Tx.Empty() && times > 0) times = SendData(Tx.Dequeue(), times);

	Set485(false);

	return times > 0;
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
	auto sp	= this;

	//!!! 只要注释这一行，四位触摸开关就不会有串口溢出错误
	if(sp->Rx.Length() == 0) return;

	// 从栈分配，节省内存
	byte buf[0x200];
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
	Set((COM)Index,  baudRate);
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
			//tsk->MaxDeepth	= 2;
			_task	= tsk;
		}
/*#if defined(STM32F0) || defined(GD32F150)
		// 打开中断
		byte irq = uart_irqs[Index];
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
    static SerialPort* _printf_sp;
	static bool isInFPutc;
}

SerialPort* SerialPort::GetMessagePort()
{
	auto sp	= _printf_sp;
	// 支持中途改变调试口
	if(sp && Sys.MessagePort != sp->Index)
	{
		delete sp;
		_printf_sp	= nullptr;
	}

	if(!sp)
	{
        auto idx	= (COM)Sys.MessagePort;
        if(idx == COM_NONE) return nullptr;

		if(isInFPutc) return nullptr;
		isInFPutc	= true;

		sp = _printf_sp = new SerialPort(idx);
		sp->Open();

		isInFPutc	= false;
	}
	return sp;
}

void SerialPort::Set485(bool flag)
{
	if(RS485)
	{
		if(!flag) Sys.Sleep(1);
		*RS485	= flag;
		if(flag) Sys.Sleep(1);
		/*if(flag)
			debug_printf("485 高\r\n");
		else
			debug_printf("485 低\r\n");*/
	}
}
