#ifndef __DAC_H__
#define __DAC_H__

/*
STM32F1：
两个DAC转换器：一个输出通道对应一个转换器
8位/12位单调输出
12位模式 左对齐 又对齐
同步更新功能
DAC同步更新  分别更新
每个通道都有DMA功能
外部触发参考
以 Vref+ 为参考
IO 设置为模拟输入（AIN）
PA4  PA5
*/

class DAConverter
{
public:
	byte	Align;

	AnalogInPort* Port = nullptr;

	byte Channel;
	DAConverter(Pin pin);

	bool Open();
	bool Close();
	bool Write(ushort value);

private:
	Pin		_Pin;

	void OnInit();
	bool OnOpen();
	bool OnClose();
	bool OnWrite(ushort value);
};

#endif
