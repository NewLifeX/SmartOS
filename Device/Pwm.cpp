#include "Sys.h"

#include "Pwm.h"

Pwm::Pwm(TIMER index) : Timer(index)
{
	for(int i=0; i<4; i++) Pulse[i] = 0xFFFF;

	Pulses		= nullptr;
	PulseCount	= 0;
	Channel		= 0;
	PulseIndex	= 0xFF;
	Repeated	= false;
	Configed	= 0x00;
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
