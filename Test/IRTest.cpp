
#include "App\IR.h"
#include "Sys.h"


/* 红外
Pwm * IRFREQ()
{
	// Pwm 引脚初始化
	AlternatePort* irpin = new AlternatePort(PA8);	
	irpin->Set(PA8);
#if defined(STM32F0) || defined(STM32F4)
	GPIO_PinAFConfig(GPIOA,GPIO_PinSource8,GPIO_AF_2);
#endif
	Pwm * IRFreq = new Pwm(0x00);		// 红外
	//  38KHz 占空比 50%
//	IRFreq->SetFrequency(38000);	// 严重不准确 103ve上实际输出只有25KHz
#if defined(STM32F0)
	IRFreq->Prescaler = 0x04;	// ok
	IRFreq->Period	= 0x13c;
#elif defined(STM32F1)
	IRFreq->Prescaler = 0x06;	// ok
	IRFreq->Period	= 0x13c;
#else
	IRFreq->Prescaler = 0x0D;	// 可能有错误
	IRFreq->Period	= 0x13c;
#endif
	IRFreq->Pulse[0]=IRFreq->Period/2;		// 红外
//	IRFreq->Start();
	return IRFreq;
}*/

//网关C 
//PB1			HIRPWM  (TIME3 CH4)
Pwm* IRFREQ()
{
	auto irpin	= new AlternatePort(PB1);	
	auto IRFreq	= new Pwm(Timer3);					
	//  38KHz 占空比 50%
//	IRFreq->SetFrequency(38000);	// 严重不准确 103ve上实际输出只有25KHz
	IRFreq->Prescaler = 0x06;	
	IRFreq->Period	= 0x13c;
	IRFreq->Pulse[3]=IRFreq->Period/2;		
//	IRFreq->Start();
	return IRFreq;
}

void IRTest()
{
	auto HIRPOWER	= new OutputPort(PE11,false,true);
	*HIRPOWER = false;
	
	/*auto ir	= new IR(IRFREQ(), PE10, PE15);
	byte Recbuff[512];
	for(int i =0;i<sizeof(Recbuff);i++)Recbuff[i]=0x00;
	int length=0;
	length = ir->Receive(Recbuff);
	
	Sys.Sleep(1000);
	while(true)
	{
		ir->Send(Recbuff,length);
		Sys.Sleep(1000);
	}*/
}
