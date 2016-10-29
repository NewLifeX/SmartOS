#include "Sys.h"
#include "Device\Port.h"
#include "Kernel\Thread.h"

Thread* th;

void ThreadTask(void* param)
{
    OutputPort* leds = (OutputPort*)param;
    uint m = Sys.Ms() % 2;
    uint n = 100;
    while(--n)
    //while(true)
    {
        *leds = !*leds;
        Sys.Sleep(m);
        //Thread::Current->Sleep(500);
    }

    Thread* th = new Thread(ThreadTask, leds);
    th->Name = "Leds";
    th->Start();
}

void TestThread(OutputPort& leds)
{
    debug_printf("\r\n");
    debug_printf("TestThread Start......\r\n");

    th = new Thread(ThreadTask, &leds);
    th->Name = "Leds";
    th->Start();

    debug_printf("\r\n TestThread Finish!\r\n");
}
