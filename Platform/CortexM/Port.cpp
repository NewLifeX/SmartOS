#include "Kernel\Sys.h"
#include "Kernel\Interrupt.h"
#include "Device\Port.h"

#include "Platform\stm32.h"

GPIO_TypeDef* IndexToGroup(byte index) { return ((GPIO_TypeDef *) (GPIOA_BASE + (index << 10))); }

/******************************** Port ********************************/

// 端口基本功能
#define REGION_Port 1
#ifdef REGION_Port

// 分组时钟
static byte _GroupClock[10];

extern void OpenPeriphClock(Pin pin, bool flag);

static void OpenClock(Pin pin, bool flag)
{
    int gi = pin >> 4;

	if(flag)
	{
		// 增加计数，首次打开时钟
		if(_GroupClock[gi]++) return;
	}
	else
	{
		// 减少计数，最后一次关闭时钟
		if(_GroupClock[gi]-- > 1) return;
	}

	OpenPeriphClock(pin, flag);
}

// 确定配置,确认用对象内部的参数进行初始化
void Port::Opening()
{
    // 先打开时钟才能配置
	OpenClock(_Pin, true);

	//auto gpio	= new GPIO_InitTypeDef();
	// 刚好4字节，不用申请内存啦
	//auto gpio	= (GPIO_InitTypeDef*)param;
	// 该结构体在F1与F0/F4的大小不一样
	GPIO_InitTypeDef gpio;
	// 特别要慎重，有些结构体成员可能因为没有初始化而酿成大错
	GPIO_StructInit(&gpio);

	OnOpen(&gpio);
}

WEAK void Port_OnOpen(Pin pin) {}

void Port::OnOpen(void* param)
{
	auto gpio	= (GPIO_InitTypeDef*)param;
    gpio->GPIO_Pin = 1 << (_Pin & 0x0F);

	Port_OnOpen(_Pin);
}

void Port::OnClose()
{
	// 不能随便关闭时钟，否则可能会影响别的引脚
	OpenClock(_Pin, false);
}

bool Port::Read() const
{
	if(_Pin == P0) return false;

	auto gp	= IndexToGroup(_Pin >> 4);
	ushort ms	= 1 << (_Pin & 0x0F);
	return GPIO_ReadInputData(gp) & ms;
}
#endif

/******************************** OutputPort ********************************/

// 输出端口
#define REGION_Output 1
#ifdef REGION_Output

bool OutputPort::Read() const
{
	if(Empty()) return false;

	auto gp	= IndexToGroup(_Pin >> 4);
	auto ms	= 1 << (_Pin & 0x0F);
	// 转为bool时会转为0/1
	bool rs = GPIO_ReadOutputData(gp) & ms;
	return rs ^ Invert;
}

void OutputPort::Write(bool value) const
{
	if(Empty()) return;

	auto gi	= IndexToGroup(_Pin >> 4);
	auto ms	= 1 << (_Pin & 0x0F);
    if(value ^ Invert)
        GPIO_SetBits(gi, ms);
    else
        GPIO_ResetBits(gi, ms);
}

// 设置端口状态
void OutputPort::Write(Pin pin, bool value)
{
	if(pin == P0) return;

	auto gi	= IndexToGroup(pin >> 4);
	auto ms	= 1 << (pin & 0x0F);
    if(value)
        GPIO_SetBits(gi, ms);
    else
        GPIO_ResetBits(gi, ms);
}

#endif

/******************************** AlternatePort ********************************/

/******************************** InputPort ********************************/

// 输入端口
#define REGION_Input 1
#ifdef REGION_Input
/* 中断状态结构体 */
/* 一共16条中断线，意味着同一条线每一组只能有一个引脚使用中断 */
typedef struct TIntState
{
    InputPort*	Port;
} IntState;

// 16条中断线
static IntState States[16];
static bool hasInitState = false;

int Bits2Index(ushort value)
{
    for(int i=0; i<16; i++)
    {
        if(value & 0x01) return i;
        value >>= 1;
    }

	return -1;
}

bool InputPort_HasEXTI(int line, const InputPort& port)
{
	return States[line].Port != nullptr && States[line].Port != &port;
}

void GPIO_ISR(int num)  // 0 <= num <= 15
{
	if(!hasInitState) return;

	auto st = &States[num];
	// 如果未指定委托，则不处理
	if(!st || !st->Port) return;

	uint bit	= 1 << num;
	bool value;
	do {
		EXTI->PR	= bit;   // 重置挂起位
		value	= st->Port->Read(); // 获取引脚状态
	} while (EXTI->PR & bit); // 如果再次挂起则重复
	// Read的时候已经计算倒置，这里不必重复计算
	st->Port->OnPress(value);
}

void SetEXIT(int pinIndex, bool enable, InputPort::Trigger mode)
{
    /* 配置EXTI中断线 */
    EXTI_InitTypeDef ext;
    EXTI_StructInit(&ext);
    ext.EXTI_Line		= EXTI_Line0 << pinIndex;
    ext.EXTI_Mode		= EXTI_Mode_Interrupt;
	if(mode == InputPort::Rising)
		ext.EXTI_Trigger	= EXTI_Trigger_Rising; // 上升沿触发
	else if(mode == InputPort::Falling)
		ext.EXTI_Trigger	= EXTI_Trigger_Falling; // 下降沿触发
	else
		ext.EXTI_Trigger	= EXTI_Trigger_Rising_Falling; // 上升沿下降沿触发

	ext.EXTI_LineCmd	= enable ? ENABLE : DISABLE;
    EXTI_Init(&ext);
}

extern void InputPort_CloseEXTI(const InputPort& port);
extern void InputPort_OpenEXTI(InputPort& port);

void InputPort::ClosePin()
{
	int idx = Bits2Index(1 << (_Pin & 0x0F));

    auto st = &States[idx];
	if(st->Port == this)
	{
		st->Port = nullptr;

		InputPort_CloseEXTI(*this);
	}
}

// 注册回调  及中断使能
bool InputPort::OnRegister()
{
    // 检查并初始化中断线数组
    if(!hasInitState)
    {
        for(int i=0; i<16; i++)
        {
            States[i].Port	= nullptr;
        }
        hasInitState = true;
    }

	int idx = Bits2Index(1 << (_Pin & 0x0F));
	auto st = &States[idx];

	auto port	= st->Port;
    // 检查是否已经注册到别的引脚上
    if(port != this && port != nullptr)
    {
#if DEBUG
        debug_printf("中断线EXTI%d 不能注册到 P%c%d, 它已经注册到 P%c%d\r\n", _Pin >> 4, _PIN_NAME(_Pin), _PIN_NAME(port->_Pin));
#endif

		// 将来可能修改设计，即使注册失败，也可以开启一个短时间的定时任务，来替代中断输入
        return false;
    }
    st->Port		= this;

    // 打开时钟，选择端口作为端口EXTI时钟线
	InputPort_OpenEXTI(*this);

	return true;
}

#endif

/******************************** AnalogInPort ********************************/
