#include "Sys.h"
#include "SerialPort.h"
#include "NRF24L01.h"
#include "TinyNet\Message.h"
#include "TinyNet\TinyClient.h"

#include "conf.h"

// 消息处理函数
bool OpenLed(Message& msg, void* param)
{
	if(msg.Reply) return true;

	OutputPort* leds = (OutputPort*)param;
	// 多组开关控制
	int i = 0;
	for(i=0; i < msg.Length && i < 3; i++)
	{
		OutputPort& led = leds[i];
		// 下面这行干嘛用？
		if(*(uint*)&led != *(uint*)&leds[0]) break;

		switch(msg.Data[i])
		{
			case 1:
			{
				debug_printf("打开开关%d\r\n", i+1);
				led = true;
				break;
			}
			case 2:
			{
				debug_printf("关闭开关%d\r\n", i+1);
				led = false;
				break;
			}
			case 3:
			{
				debug_printf("闪烁开关%d\r\n", i+1);
				led = !led;
				break;
			}
		}
	}
	// 返回实际灯数
	if(msg.Length > i) msg.Length = i;

	return true;
}

// 发送刷小灯的消息
void FlashLed(void* param)
{
	assert_ptr(param);
	Controller* control = (Controller*)param;

	// 发送广播消息，刷所有人小灯
	Message msg(0x10);

	byte leds[] = {3, 2, 1};
	leds[1] = Time.Current() % 4;
	leds[2] = Time.Current() % 4;

	msg.SetData(leds, ArrayLength(leds));

	control->Send(msg);
}

void TestMessage(OutputPort* leds)
{
    debug_printf("\r\n");
    debug_printf("TestMessage Start......\r\n");

	NRF24L01* nrf = Create2401();
    //nrf->Timeout = 1000;
    //nrf->Channel = 0;
	// 如果打开自动应答，繁忙时收不到数据会增加错误计数，达到一定程度会自动重启模块
    nrf->AutoAnswer = false;
	nrf->Speed = 250;
    if(!nrf->Check())
        debug_printf("请检查NRF24L01线路\r\n");

    // 使用nRF24L01+作为链路层
    Controller* control = new Controller(nrf);

	TinyClient* client = new TinyClient(control);
	client->DeviceType = 0x0101;
	//client->SetDefault();

	// 注册消息。每个消息代码对应一个功能函数
	control->Register(0x10, OpenLed, leds);

	// 添加定时任务
	debug_printf("灯光闪烁任务 ");
	Sys.AddTask(FlashLed, control, 0, 2000000);

    debug_printf("\r\n TestMessage Finish!\r\n\r\n");
}
