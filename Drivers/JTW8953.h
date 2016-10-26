#ifndef _JTW8953_H_
#define _JTW8953_H_

class I2C;
class InputPort;

// 滑条按键触摸芯片
class JTW8953
{
public:
	I2C* 		IIC;		// I2C通信口
	InputPort*	Port;		// 中断引脚
	ushort		Address;	// 设备地址

	JTW8953();
	~JTW8953();

	void Init();
	void SetSlide(bool slide);
	// 写入键位数据
	bool WriteKey(ushort index, byte data);

	// 设置配置
	bool SetConfig(const Buffer& bs) const;

	bool Write(const Buffer& bs) const;

	bool Read(Buffer& bs) const;

	static void JTW8953Test();

private:
};

#endif
