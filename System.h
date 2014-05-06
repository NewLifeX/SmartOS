/*
  * 全新的系统API架构
  *
  */

#ifndef _SYSTEM_H_
#define _SYSTEM_H_

typedef char                sbyte;
typedef unsigned char       byte;
typedef unsigned short      ushort;
typedef unsigned int        uint;
typedef char*               string;
typedef unsigned char       bool;
#define true                1
#define false               0

typedef struct
{
	byte Group;
	byte Port;
} Pin;

typedef struct
{
	void (*Init)(void);
	void (*Uninit)(void);

    void (*Printf)(const string format, ...);
    void (*LcdPrintf)(const string format,...);
    void* (*Malloc)(uint len);
    void (*Free)(void* ptr);
    void (*Sleep)(uint ms);
    void (*Delay)(uint us);
    void (*DisableInterrupts)();
    void (*EnableInterrupts)();
    uint (*WaitForEvents)(uint wakeupSystemEvents, uint timeout_Milliseconds);
    uint (*ComputeCRC)(const void* rgBlock, int nLength, uint crc);
} TCore;

typedef struct
{
	void (*Init)(void);
	void (*Uninit)(void);

} TBoot;

typedef struct
{
	void (*Init)(void);
	void (*Uninit)(void);

    int (*snprintf)(string buffer, uint len, const string format, ...);
    int (*stricmp)(const string dst, const string src);
    int (*strncmp)(const string str1, const string str2, uint num);
    uint (*strlen)(const string str);
    void *(*memcpy)(void * dst, const void * src, uint len);
    void *(*memset)(void * dst, int value, uint len);
} TMem;

typedef struct
{
	void (*Init)(void);
	void (*Uninit)(void);

    int (*Erase)(uint address, uint count);
    int (*Read)(uint address, uint count,byte *buffer);
    int (*Write)(uint address, uint count,byte *buffer);
} TFlash;

typedef struct
{
	void (*Init)(void);
	void (*Uninit)(void);

    //void (*DisablePin)(Pin pin, GPIO_RESISTOR ResistorState, uint Direction, GPIO_ALT_MODE AltFunction);
    //bool (*EnableInputPin)(Pin pin, bool GlitchFilterEnable, GPIO_INTERRUPT_SERVICE_ROUTINE ISR, GPIO_INT_EDGE IntEdge, GPIO_RESISTOR ResistorState);
    void (*EnableOutputPin)(Pin pin, bool initialState);
    bool (*GetPinState)(Pin pin);
    void (*SetPinState)(Pin pin, bool state);
} TIO;

typedef struct
{
	void (*Init)(void);
	void (*Uninit)(void);

    bool (*Initialize)(int ComPortNum, int BaudRate, int Parity, int DataBits, int StopBits, int FlowValue);
    bool (*Uninitialize)(int ComPortNum);
    int  (*Write)(int ComPortNum, const string Data, uint size);
    int  (*Read)(int ComPortNum, string Data, uint size);
    bool (*Flush)(int ComPortNum);
    int  (*BytesInBuffer)(int ComPortNum, bool fRx);
    void (*DiscardBuffer)(int ComPortNum, bool fRx);
} TUsart;

typedef struct
{
	void (*Init)(void);
	void (*Uninit)(void);

	//bool (*DA_Initialize)(ANALOG_CHANNEL channel, int precisionInBits);
	//void (*DA_Write)(ANALOG_CHANNEL channel, int level);
	//bool (*AD_Initialize)(ANALOG_CHANNEL channel, int precisionInBits);
	//int (*AD_Read)(ANALOG_CHANNEL channel);
} TAnalog;

typedef struct
{
	void (*Init)(void);
	void (*Uninit)(void);

    //bool (*Initialize)(PWM_CHANNEL channel);
    //bool (*Uninitialize)(PWM_CHANNEL channel);
    //bool (*ApplyConfiguration)(PWM_CHANNEL channel, Pin pin, uint& period, uint& duration, PWM_SCALE_FACTOR &scale, bool invert);
    //bool (*Start)(PWM_CHANNEL channel, Pin pin);
    //void (*Stop)(PWM_CHANNEL channel, Pin pin);
    //Pin (*GetPinForChannel)(PWM_CHANNEL channel);
} TPwm;

typedef struct
{
	void (*Init)(void);
	void (*Uninit)(void);

    //bool (*WriteRead)(const SPI_CONFIGURATION& Configuration, byte* Write8, int WriteCount, byte* Read8, int ReadCount, int ReadStartOffset);
    //bool (*WriteRead16)(const SPI_CONFIGURATION& Configuration, ushort* Write16, int WriteCount, ushort* Read16, int ReadCount, int ReadStartOffset);
} TSpi;

typedef struct
{
	void (*Init)(void);
	void (*Uninit)(void);

    bool (*Initialize)();
    bool (*Uninitialize)();
    bool (*Execute)(ushort address,byte *inBuffer,int inCount,byte *outBuffer,int outCount,uint clockRateKhz,int timeout);
} TI2c;

typedef struct
{
	void (*Init)(void);
	void (*Uninit)(void);

    void (*Clear)(uint color);
    void (*SetPixel)(int x,int y,uint color);
    uint (*GetPixel)(int x,int y);
    void (*DrawLine)(int x1,int y1,int x2,int y2,uint color);
    void (*DrawRectangle)(int x,int y,int width,int height,uint color);
    void (*DrawEllipse)(int x,int y,int width,int height,uint color);
    void (*DrawImage)(int x,int y,byte *bytData);
    void (*DrawImageEx)(int x,int y,byte *bytData,uint MaskColor);
    void (*DrawString)(int x,int y, string s,uint color);
    void (*DrawStringEx)(int x,int y,uint color,byte *fontdata,int width,int height,int count);
    void (*FillRectangle)(int x,int y,int width,int height,uint color);
    void (*FillEllipse)(int x,int y,int width,int height,uint color);
    void (*GetFrameBufferEx)(byte *bytData,uint offset,uint size);
    void (*SuspendLayout)();
    void (*ResumeLayout)();
} TLcd;

// 全局系统根
// 采用指针而不是对象，主要是考虑到每个模块可能扩展，而不能影响系统根的其它部分
typedef struct
{
	void (*Init)(void);
	void (*Uninit)(void);

	TBoot Boot;
	TCore Core;
	TMem Mem;
	TFlash Flash;
	TIO IO;
	TUsart Usart;
	TAnalog Analog;
	TPwm Pwm;
	TSpi Spi;
	TI2c I2c;
	TLcd Lcd;
} TSystem;

// 声明全局的Sys根对象
extern TSystem Sys;
extern void SysInit(void);

#endif //_SYSTEM_H_
