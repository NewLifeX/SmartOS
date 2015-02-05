
#include "Sound.h"

Music::Music(Timer* timer,OutputPort* pin)
{
	Init();
	if(timer!= NULL)
		_timer = timer;
	if(pin!= NULL)
		_phonatePin = pin;
}

void Music::Init()
{
	_tuneSet = NULL;
	_tuneNum = 0;
	_timer = NULL;
	_phonatePin = NULL;
	Sounding = false;
	sound_cnt = 0;
	music_beat = 0;
	music_freq = 0;
}

void Music::Disposable()
{
	if(_timer)
	{
		_timer->Stop();
		delete(_timer);
	}
	if(_phonatePin)delete(_phonatePin);
	_tuneSet = NULL;
}


Music::~Music()
{
	_tuneSet = NULL;
	_tuneNum = NULL;
	_timer->Stop();
	Sounding = false;
	_timer = NULL;
}

void Music::Sound()
{
	if(_timer!= NULL && _phonatePin != NULL && _tuneSet != NULL && _tuneNum != 0)
	{
		_timer->SetFrequency(100000);
		_timer->Register(TimerHander,this);
		_timer->Start();
		Sounding = true;
	}
}

void Music::Sound(Tune *tune,int num)
{
	SetTuneSet(tune,num);
	Sound();
}

void Music::Unsound()
{
	_timer->Stop();
	Sounding = false;
}

void Music::SetTuneSet(Tune * tune,int num)
{
	if(Sounding)
		Unsound();
	if(tune != NULL && num != NULL)
	{
		_tuneSet = tune;
		_tuneNum = num;
	}
}

bool Music::getStat()
{
	return Sounding;
}

void Music::TimerHander(void *sender,void *param)
{
	if(param == NULL)return;
	Music * music = (Music * )param;
	music->phonate();
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
