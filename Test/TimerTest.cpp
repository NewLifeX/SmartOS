#include "Sys.h"
#include "Port.h"
#include "Timer.h"

Timer* timer;

void TimerTask(void* sender, void* param)
{
    OutputPort* leds = (OutputPort*)param;
    *leds = !*leds;
}

uint frequency = 0;
int step = 1;

void TimerTask2(void* sender, void* param)
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

    timer->SetFrequency(frequency);
}

void TestTimer(OutputPort& leds)
{
    debug_printf("\r\n");
    debug_printf("TestTimer Start......\r\n");

    timer = new Timer(Timer2);
    timer->SetFrequency(50);
    timer->Register(TimerTask, &leds);
    timer->Open();

    Timer* timer2 = Timer::Create();
    timer2->SetFrequency(10);
    timer2->Register(TimerTask2, NULL);
    timer2->Open();

    debug_printf("\r\n TestTimer Finish!\r\n");
}
