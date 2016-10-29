#include "Platform\Pin.h"
#include "SerialPort.h"

#ifdef DEBUG
static uint OnUsartRead(ITransport* transport, Buffer& bs, void* param, void* param2)
{
	auto sp	= (SerialPort*)param;
	debug_printf("%s 收到：", sp->Name);
	bs.Show(true);
	bs.AsString().Show(true);

	// 原路发回去
	// 部分核心板COM4用于WiFi模块
	if(!bs.AsString().Contains("ERROR"))
	{
		String str	= sp->Name;
		str	+= " 收到：";
		str	+= bs.AsString();
		sp->Write(str);
	}

    return 0;
}

static void TestSerialTask(void* param)
{
    debug_printf("\r\n\r\n");
    debug_printf("测试串口开始......\r\n");

	COM coms[]	= {COM2, COM3, COM4, COM5};
	List<SerialPort*> list;

	// 创建待测试对象
	for(int i=0; i<ArrayLength(coms); i++)
	{
		auto sp	= new SerialPort(coms[i], 115200);
		sp->Register(OnUsartRead, sp);
		// COM5是RS485
		if(coms[i] == COM5) sp->RS485	= new OutputPort(PC9, false);
		sp->Open();

		list.Add(sp);
	}

	// 串口输出
	String str = "万家灯火，无声物联！\r\n";
	debug_printf("向所有串口输出：");
	str.Show();
	for(int k=0; k<5; k++)
	{
		debug_printf("第 %d 次输出\r\n", k+1);
		for(int i=0; i<list.Count(); i++)
		{
			auto sp	= list[i];
			sp->Write(str);
		}
		Sys.Sleep(1000);
	}

    /*// 等待输入
    Sys.Sleep(60000);

	// 销毁
	list.DeleteAll();*/

    debug_printf("\r\n测试串口完成\r\n\r\n");
}

void SerialPort::Test()
{
    Sys.AddTask(TestSerialTask, nullptr, 1000, -1, "串口测试");
}
#endif
