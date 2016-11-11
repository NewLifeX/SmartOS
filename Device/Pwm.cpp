#include "Sys.h"

#include "Port.h"
#include "Pwm.h"

Pwm::Pwm(TIMER index) : Timer(index)
{
	for(int i=0; i<4; i++)
	{
		Pulse[i]	= 0xFFFF;
		Ports[i]	= nullptr;
		Inited[i]	= false;
	}

	Remap		= 0;

	Pulses		= nullptr;
	PulseCount	= 0;
	Channel		= 0;
	PulseIndex	= 0xFF;
	Repeated	= false;
}

Pwm::~Pwm()
{
	for(int i=0; i<4; i++)
	{
		delete Ports[i];
	}
}

void Pwm::SetPulse(int idx, ushort pulse)
{
	if(Pulse[idx] == pulse) return;

	Pulse[idx] = pulse;

	if(Opened)
		Config();
	else
		Open();
}
