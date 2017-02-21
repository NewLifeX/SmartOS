#include "Kernel\Sys.h"
#include "Device\Port.h"
#include "Device\Timer.h"

//Timer* timer;

void TimerTask(OutputPort& led, Timer& timer)
{
    led = !led;
}

uint frequency = 0;
int step = 1;

void TimerTask2(Timer& timer)
{
	frequency += step;
	
	if(frequency <= 0)
	{
		frequency = 1;
		step = 1;
	}
	else if(frequency >= 60)
	{
		//frequency = 100;
		step = -1;
	}

    timer.SetFrequency(frequency);
}

void TestTimer(OutputPort& led)
{
    debug_printf("\r\n");
    debug_printf("TestTimer Start......\r\n");

    auto timer	= new Timer(Timer2);
    timer->SetFrequency(50);
    //timer->Register(TimerTask, &led);
	timer->Register(Delegate<Timer&>(TimerTask, &led));
    timer->Open();

    auto timer2	= Timer::Create();
    timer2->SetFrequency(10);
    //timer2->Register(TimerTask2, nullptr);
	timer2->Register(TimerTask2);
    timer2->Open();

    debug_printf("\r\n TestTimer Finish!\r\n");
}
