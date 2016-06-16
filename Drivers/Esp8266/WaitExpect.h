#ifndef __WaitExpect_H__
#define __WaitExpect_H__

// 等待
class WaitExpect
{
public:
	const String*	Command	= nullptr;
	String*	Result	= nullptr;
	cstring	Key1	= nullptr;
	cstring	Key2	= nullptr;

	bool	Capture	= true;	// 是否捕获所有

	bool Wait(int msTimeout);
	uint Parse(const Buffer& bs);
	uint FindKey(const String& str);
};

#endif
