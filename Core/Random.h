#ifndef __Random_H__
#define __Random_H__

// 随机数
class Random
{
public:
	Random();
	Random(uint seed);

	uint Next() const;
	uint Next(uint max) const;
	void Next(Buffer& bs) const;
};

#endif
