#ifndef __WaitHandle_H__
#define __WaitHandle_H__

// 等待句柄
class WaitHandle
{
public:
	bool	Result;	// 结果

	WaitHandle();
	
	bool WaitOne(int ms);	// 等待一个
	
	//void Reset();
	void Set();	// 设置结果
	
private:
};

#endif
