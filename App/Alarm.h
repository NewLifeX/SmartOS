#ifndef __Alarm_H__
#define __Alarm_H__

#include "Sys.h"
#include "Timer.h"
#include "Port.h"

typedef struct		// 单个音节
{
	byte freq;
	byte timer;
}Tune;

class Alarm
{
private:
	const Tune*	_tuneSet;		// 调子集合 就是曲子
	int			_tuneNum;		// 有多少个调子
	Timer*		_timer;			// 定时器
	OutputPort*	_phonatePin;	// 输出引脚

	bool Sounding;			// 作为判断的依据  最好只有Get 没有Set
public:	
	bool getStat();

	Alarm(Timer*,OutputPort*);
	void Init();
	~Alarm();
	void Sound();
	void Unsound();
	void Sound(const Tune*,int num = 0);
	void SetTuneSet(const Tune*,int num = 0);
	void Disposable();

private:
	volatile int sound_cnt;
	volatile int music_freq;
	volatile int music_beat;
	void TimerHander(Timer& timer);
	void phonate();
};

#endif 
