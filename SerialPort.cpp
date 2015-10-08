#include "Sys.h"
#include <stdio.h>
#include "Time.h"

#include "Port.h"
#include "SerialPort.h"

#define COM_DEBUG 0

SerialPort::SerialPort() { Init(); }

SerialPort::SerialPort(USART_TypeDef* com, int baudRate, byte parity, byte dataBits, byte stopBits)
{
	assert_param(com);

	const USART_TypeDef* const g_Uart_Ports[] = UARTS;
	byte _index = 0xFF;
	for(int i=0; i<ArrayLength(g_Uart_Ports); i++)
	{
		if(g_Uart_Ports[i] == com)
		{
			_index = i;
			break;
		}
	}

	Init();
	Init(_index, baudRate, parity, dataBits, stopBits);
}

// 析构时自动关闭
SerialPort::~SerialPort()
{
	if(RS485) delete RS485;
	RS485 = NULL;
}

void SerialPort::Init()
{
	_index	= 0xFF;
	RS485	= NULL;
	Error	= 0;

#ifdef STM32F1XX
	IsRemap	= false;
#endif
	MinSize	= 1;

	_taskidRx	= 0;
}

void SerialPort::Init(byte index, int baudRate, byte parity, byte dataBits, byte stopBits)
{
	USART_TypeDef* const g_Uart_Ports[] = UARTS;
	_index = index;
	assert_param(_index < ArrayLength(g_Uart_Ports));

    _port		= g_Uart_Ports[_index];
    _baudRate	= baudRate;
    _parity		= parity;
    _dataBits	= dataBits;
    _stopBits	= stopBits;

	// 计算字节间隔。字节速度一般是波特率转为字节后再除以2
	//_byteTime = 15000000 / baudRate;  // (1000000 /(baudRate/10)) * 1.5
	_byteTime	= 1000000 / (baudRate >> 3 >> 1);
	_byteTime	<<= 1;

	// 根据端口实际情况决定打开状态
	if(_port->CR1 & USART_CR1_UE) Opened = true;

	// 设置名称
	//Name = "COMx";
	memcpy((byte*)Name, (byte*)"COMx", 4);
	Name[3] = '0' + _index + 1;
	Name[4] = 0;
}

// 打开串口
bool SerialPort::OnOpen()
{
    Pin rx, tx;
    GetPins(&tx, &rx);

    //debug_printf("Serial%d Open(%d, %d, %d, %d)\r\n", _index + 1, _baudRate, _parity, _dataBits, _stopBits);
#if COM_DEBUG
    if(_index != Sys.MessagePort)
    {
ShowLog:
        debug_printf("Serial%d Open(%d", _index + 1, _baudRate);
        switch(_parity)
        {
            case USART_Parity_No: debug_printf(", Parity_None"); break;
            case USART_Parity_Even: debug_printf(", Parity_Even"); break;
            case USART_Parity_Odd: debug_printf(", Parity_Odd"); break;
        }
        switch(_dataBits)
        {
            case USART_WordLength_8b: debug_printf(", WordLength_8b"); break;
            case USART_WordLength_9b: debug_printf(", WordLength_9b"); break;
        }
        switch(_stopBits)
        {
#ifdef STM32F10X
            case USART_StopBits_0_5: debug_printf(", StopBits_0_5"); break;
#endif
            case USART_StopBits_1: debug_printf(", StopBits_1"); break;
            case USART_StopBits_1_5: debug_printf(", StopBits_1_5"); break;
            case USART_StopBits_2: debug_printf(", StopBits_2"); break;
        }
        debug_printf(") TX=P%c%d RX=P%c%d\r\n", _PIN_NAME(tx), _PIN_NAME(rx));

        // 有可能是打开串口完成以后跳回来
        if(Opened) return true;
    }
#endif

	USART_InitTypeDef  p;

	//串口引脚初始化
    _tx.Set(tx).Open();
#if defined(STM32F0) || defined(STM32F4)
    _rx.Set(rx).Open();
#else
    _rx.Set(rx).Open();
#endif

	// 不要关调试口，否则杯具
    if(_index != Sys.MessagePort) USART_DeInit(_port);
	// USART_DeInit其实就是关闭时钟，这里有点多此一举。但为了安全起见，还是使用

	// 检查重映射
#ifdef STM32F1XX
	if(IsRemap)
	{
		switch (_index) {
		case 0: AFIO->MAPR |= AFIO_MAPR_USART1_REMAP; break;
		case 1: AFIO->MAPR |= AFIO_MAPR_USART2_REMAP; break;
		case 2: AFIO->MAPR |= AFIO_MAPR_USART3_REMAP_FULLREMAP; break;
		}
	}
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE );
#endif

    // 打开 UART 时钟。必须先打开串口时钟，才配置引脚
#ifdef STM32F0XX
	switch(_index)
	{
		case COM1:	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);	break;
		case COM2:	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);	break;
		default:	break;
	}
#else
	if (_index) { // COM2-5 on APB1
        RCC->APB1ENR |= RCC_APB1ENR_USART2EN >> 1 << _index;
    } else { // COM1 on APB2
        RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    }
#endif

#ifdef STM32F0
	_tx.AFConfig(GPIO_AF_1);
	_rx.AFConfig(GPIO_AF_1);
#elif defined(STM32F4)
	const byte afs[] = { GPIO_AF_USART1, GPIO_AF_USART2, GPIO_AF_USART3, GPIO_AF_UART4, GPIO_AF_UART5, GPIO_AF_USART6, GPIO_AF_UART7, GPIO_AF_UART8 };
	_tx.AFConfig(afs[_index]);
	_rx.AFConfig(afs[_index]);
#endif

    USART_StructInit(&p);
	p.USART_BaudRate = _baudRate;
	p.USART_WordLength = _dataBits;
	p.USART_StopBits = _stopBits;
	p.USART_Parity = _parity;
	USART_Init(_port, &p);

	// 串口接收中断配置，同时会打开过载错误中断
	USART_ITConfig(_port, USART_IT_RXNE, ENABLE);
	//USART_ITConfig(_port, USART_IT_PE, ENABLE);
	//USART_ITConfig(_port, USART_IT_ERR, ENABLE);
	//USART_ITConfig(_port, USART_IT_TXE, DISABLE);

	// 清空缓冲区
	Tx.Clear();
	Rx.Clear();

	// 打开中断，收发都要使用
	const byte irqs[] = UART_IRQs;
	byte irq = irqs[_index];
	Interrupt.SetPriority(irq, 1);
	Interrupt.Activate(irq, OnHandler, this);

	USART_Cmd(_port, ENABLE);//使能串口

	if(RS485) *RS485 = false;

    //Opened = true;

#if COM_DEBUG
    if(_index == Sys.MessagePort)
	{
		// 提前设置为已打开端口，ShowLog里面需要判断
		Opened = true;
		goto ShowLog;
	}
#endif

	return true;
}

// 关闭端口
void SerialPort::OnClose()
{
    debug_printf("~Serial%d Close\r\n", _index + 1);

	USART_Cmd(_port, DISABLE);
    USART_DeInit(_port);

    _tx.Close();
	_rx.Close();

	const byte irqs[] = UART_IRQs;
	byte irq = irqs[_index];
	Interrupt.Deactivate(irq);

	// 检查重映射
#ifdef STM32F1XX
	if(IsRemap)
	{
		switch (_index) {
		case 0: AFIO->MAPR &= ~AFIO_MAPR_USART1_REMAP; break;
		case 1: AFIO->MAPR &= ~AFIO_MAPR_USART2_REMAP; break;
		case 2: AFIO->MAPR &= ~AFIO_MAPR_USART3_REMAP_FULLREMAP; break;
		}
	}
#endif
}

// 发送单一字节数据
uint SerialPort::SendData(byte data, uint times)
{
    while(USART_GetFlagStatus(_port, USART_FLAG_TXE) == RESET && --times > 0);//等待发送完毕
    if(times > 0)
		USART_SendData(_port, (ushort)data);
	else
		Error++;

	return times;
}

// 向某个端口写入数据。如果size为0，则把data当作字符串，一直发送直到遇到\0为止
bool SerialPort::OnWrite(const ByteArray& bs)
{
	if(!bs.Length()) return true;
#ifdef STM32F0
	if(RS485) *RS485 = true;
	// 中断发送过于频繁，影响了接收中断，采用循环阻塞发送。后面考虑独立发送任务
	for(int i=0; i<bs.Length(); i++)
	{
		SendData(bs[i], 3000);
	}
	if(RS485) *RS485 = false;
#else
	// 如果队列已满，则强制刷出
	if(Tx.Length() + bs.Length() > Tx.Capacity()) Flush(Sys.Clock / 40000);

	Tx.Write(bs);

	// 打开串口发送
	if(RS485) *RS485 = true;
	USART_ITConfig(_port, USART_IT_TXE, ENABLE);
#endif

	return true;
}

// 刷出某个端口中的数据
bool SerialPort::Flush(uint times)
{
	// 打开串口发送
	if(RS485) *RS485 = true;
	//USART_ITConfig(_port, USART_IT_TXE, ENABLE);

	// 打开中断
	//SmartIRQ irq(true);

    //while(USART_GetFlagStatus(_port, USART_FLAG_TXE) == RESET && --times > 0);//等待发送完毕
	while(!Tx.Empty() && times > 0) times = SendData(Tx.Pop(), times);

	return times > 0;
}

void SerialPort::OnTxHandler()
{
	if(!Tx.Empty())
	{
		USART_SendData(_port, (ushort)Tx.Pop());
	}
	else
	{
		USART_ITConfig(_port, USART_IT_TXE, DISABLE);

		if(RS485) *RS485 = false;
	}

	// USART_SendData会清中断，这里不需要
	//USART_ClearITPendingBit(_port, USART_IT_TXE);
}

// 从某个端口读取数据
uint SerialPort::OnRead(ByteArray& bs)
{
	// 如果有数据变化，等一会
	uint count = 0;
	uint len = Rx.Length();
	while(len != count)
	{
		count = len;
		// 按照115200波特率计算，传输7200字节每秒，每个毫秒7个字节，大概150微秒差不多可以接收一个新字节
		Sys.Delay(_byteTime);
		len = Rx.Length();
	}
	// 如果数据大小不足，等下次吧
	if(len < MinSize) return 0;

	//debug_printf("串口接收缓冲区 %d 间隔 %dus 最小 %d ", len, _byteTime, MinSize);
	// 从接收队列读取
	count = Rx.Read(bs);
	//debug_printf(" 读取 %d \r\n", count);
	bs.SetLength(count);

	// 如果还有数据，打开任务
	if(!Rx.Empty()) Sys.SetTask(_taskidRx, true);

	return count;
}

void SerialPort::OnRxHandler()
{
	// 串口接收中断必须以极快的速度完成，否则会出现丢数据的情况
	// 判断缓冲区足够最小值以后才唤醒任务，减少时间消耗
	// 缓冲区里面别用%，那会产生非常耗时的除法运算
	byte dat = (byte)USART_ReceiveData(_port);
	Rx.Push(dat);

	// 收到数据，开启任务调度。延迟_byteTime，可能还有字节到来
	//!!! 暂时注释任务唤醒，避免丢数据问题
	if(_taskidRx && Rx.Length() >= MinSize)
	{
		//Sys.SetTask(_taskidRx, true, (_byteTime >> 10) + 1);
		_task->NextTime	= Time.Current() + 1;
		_task->Enable	= true;
		// 如果系统调度器处于Sleep，让它立马退出
		Task::Scheduler()->Sleeping = false;
	}
}

void SerialPort::ReceiveTask(void* param)
{
	SerialPort* sp = (SerialPort*)param;
	assert_param2(sp, "串口参数不能为空 ReceiveTask");

	// 从栈分配，节省内存
	ByteArray bs(0x40);
	uint len = sp->Read(bs);
	if(len)
	{
		len = sp->OnReceive(bs, NULL);
		//assert_param(len <= ArrayLength(buf));
		// 如果有数据，则反馈回去
		if(len) sp->Write(bs);
	}
}

void SerialPort::SetBaudRate(int baudRate)
{
	Init( _index,  baudRate,  _parity,  _dataBits,  _stopBits);
}

void SerialPort::Register(TransportHandler handler, void* param)
{
	ITransport::Register(handler, param);

    if(handler)
	{
		// 建立一个未启用的任务，用于定时触发接收数据，收到数据时开启
		if(!_taskidRx)
		{
			_taskidRx = Sys.AddTask(ReceiveTask, this, 1000, 500, "串口接收");
			_task = Task::Get(_taskidRx);
		}
	}
    else
	{
		Sys.RemoveTask(_taskidRx);
	}
}

// 真正的串口中断函数
void SerialPort::OnHandler(ushort num, void* param)
{
	SerialPort* sp = (SerialPort*)param;
	//assert_param2(sp, "串口参数不能为空 OnHandler");

	if(USART_GetITStatus(sp->_port, USART_IT_TXE) != RESET) sp->OnTxHandler();
	// 接收中断
	if(USART_GetITStatus(sp->_port, USART_IT_RXNE) != RESET) sp->OnRxHandler();
	// 溢出
	if(USART_GetFlagStatus(sp->_port, USART_FLAG_ORE) != RESET)
	{
		USART_ClearFlag(sp->_port, USART_FLAG_ORE);
		// 读取并扔到错误数据
		//USART_ReceiveData(sp->_port);
		debug_printf("Serial%d 溢出 MinSize=%d \r\n", sp->_index + 1, sp->MinSize);
	}
	/*if(USART_GetFlagStatus(sp->_port, USART_FLAG_NE) != RESET) USART_ClearFlag(sp->_port, USART_FLAG_NE);
	if(USART_GetFlagStatus(sp->_port, USART_FLAG_FE) != RESET) USART_ClearFlag(sp->_port, USART_FLAG_FE);
	if(USART_GetFlagStatus(sp->_port, USART_FLAG_PE) != RESET) USART_ClearFlag(sp->_port, USART_FLAG_PE);*/
}

// 获取引脚
void SerialPort::GetPins(Pin* txPin, Pin* rxPin)
{
    *rxPin = *txPin = P0;

	const Pin g_Uart_Pins[] = UART_PINS;
	const Pin* p = g_Uart_Pins;
#ifdef STM32F1XX
	const Pin g_Uart_Pins_Map[] = UART_PINS_FULLREMAP;
	if(IsRemap) p = g_Uart_Pins_Map;
#endif

	int n = _index << 2;
	*txPin  = p[n];
	*rxPin  = p[n + 1];
}

extern "C"
{
    SerialPort* _printf_sp;
	bool isInFPutc;

    /* 重载fputc可以让用户程序使用printf函数 */
    int fputc(int ch, FILE *f)
    {
        if(Sys.Clock == 0) return ch;

        int _index = Sys.MessagePort;
        if(_index == COM_NONE) return ch;

		USART_TypeDef* g_Uart_Ports[] = UARTS;
        USART_TypeDef* port = g_Uart_Ports[_index];

		if(isInFPutc) return ch;
		isInFPutc = true;
        // 检查并打开串口
        if((port->CR1 & USART_CR1_UE) != USART_CR1_UE && _printf_sp == NULL)
        {
            _printf_sp = SerialPort::GetMessagePort();
        }

		if(_printf_sp)
		{
			_printf_sp->Write(ByteArray(ch, 1));
		}

		isInFPutc = false;
        return ch;
    }
}

SerialPort* SerialPort::GetMessagePort()
{
	if(!_printf_sp)
	{
        int _index = Sys.MessagePort;
        if(_index == COM_NONE) return NULL;

		USART_TypeDef* g_Uart_Ports[] = UARTS;
        USART_TypeDef* port = g_Uart_Ports[_index];
		_printf_sp = new SerialPort(port);
#if DEBUG
#ifdef STM32F0
		//_printf_sp->Tx.SetCapacity(256);
#else
		//_printf_sp->Tx.SetCapacity(1024);
#endif
#endif
		_printf_sp->Open();

		//char str[] = "                                \r\n";
		//_printf_sp->Write((byte*)str, ArrayLength(str));
	}
	return _printf_sp;
}
