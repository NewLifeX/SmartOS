
#include "Sound.h"

Music::Music(Timer* timer,OutputPort* pin)
{
	Init();
	if(timer!= nullptr)
		_timer = timer;
	if(pin!= nullptr)
		_phonatePin = pin;
}

void Music::Init()
{
	_tuneSet = nullptr;
	_tuneNum = 0;
	_timer = nullptr;
	_phonatePin = nullptr;
	Sounding = false;
	sound_cnt = 0;
	music_beat = 0;
	music_freq = 0;
}

void Music::Disposable()
{
	if(_timer)
	{
		_timer->Close();
		delete(_timer);
	}
	if(_phonatePin)delete(_phonatePin);
	_tuneSet = nullptr;
}


Music::~Music()
{
	_tuneSet	= nullptr;
	_tuneNum	= 0;
	_timer->Close();
	Sounding	= false;
	_timer		= nullptr;
}

void Music::Sound()
{
	if(_timer!= nullptr && _phonatePin != nullptr && _tuneSet != nullptr && _tuneNum != 0)
	{
		_timer->SetFrequency(100000);
		//_timer->Register(TimerHander, this);
		_timer->Register(Delegate<Timer&>(&Music::TimerHander, this));
		_timer->Open();
		Sounding = true;
	}
}

void Music::Sound(const Tune* tune,int num)
{
	SetTuneSet(tune,num);
	Sound();
}

void Music::Unsound()
{
	_timer->Close();
	Sounding = false;
}

void Music::SetTuneSet(const Tune* tune, int num)
{
	if(Sounding) Unsound();

	if(tune != nullptr && num != 0)
	{
		_tuneSet = tune;
		_tuneNum = num;
	}
}

bool Music::getStat()
{
	return Sounding;
}

void Music::TimerHander(Timer& timer)
{
	/*if(param == nullptr)return;
	Music * music = (Music * )param;
	music->phonate();*/

	phonate();
}

void Music::phonate()
{
	if(music_freq == 0)
	{
		music_freq = _tuneSet[sound_cnt].freq;
		*_phonatePin = !*_phonatePin;
	}
	if(music_beat == 0)
	{
		sound_cnt++;
		if(sound_cnt >= _tuneNum)
			sound_cnt = 0 ;
		music_beat = _tuneSet[sound_cnt].timer * 700;
		music_freq = _tuneSet[sound_cnt].freq;
	}
	music_freq --;
	music_beat --;
}
