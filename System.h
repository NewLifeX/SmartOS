/*
  * 全新的系统API架构
  *
  */

#ifndef _SYSTEM_H_
#define _SYSTEM_H_

/* 类型定义 */
typedef char            sbyte;
typedef unsigned char   byte;
typedef unsigned short  ushort;
typedef unsigned int    uint;
typedef char*           string;
typedef unsigned char   bool;
#define true            1
#define false           0

/* 引脚定义 */
typedef ushort			Pin;
/*typedef struct
{
	byte Group;
	byte Port;
} TPin;*/
#include "Pin.h"

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

extern void TCore_Init(void);

typedef struct
{
	void (*Init)(void);
	void (*Uninit)(void);

} TBoot;

extern void TBoot_Init(void);

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

extern void TMem_Init(void);

typedef struct
{
	void (*Init)(void);
	void (*Uninit)(void);

    int (*Erase)(uint address, uint count);
    int (*Read)(uint address, uint count,byte *buffer);
    int (*Write)(uint address, uint count,byte *buffer);
} TFlash;

extern void TFlash_Init(void);

typedef struct
{
	void (*Init)(void);
	void (*Uninit)(void);

    //void (*DisablePin)(Pin pin, GPIO_RESISTOR ResistorState, uint Direction, GPIO_ALT_MODE AltFunction);
    //bool (*EnableInputPin)(Pin pin, bool GlitchFilterEnable, GPIO_INTERRUPT_SERVICE_ROUTINE ISR, GPIO_INT_EDGE IntEdge, GPIO_RESISTOR ResistorState);
    void (*EnableOutputPin)(Pin pin, bool initialState);
    bool (*Get)(Pin pin);
    void (*Set)(Pin pin, bool state);
} TIO;

extern void TIO_Init(void);

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

extern void TUsart_Init(void);

typedef struct
{
	void (*Init)(void);
	void (*Uninit)(void);

	//bool (*DA_Initialize)(ANALOG_CHANNEL channel, int precisionInBits);
	//void (*DA_Write)(ANALOG_CHANNEL channel, int level);
	//bool (*AD_Initialize)(ANALOG_CHANNEL channel, int precisionInBits);
	//int (*AD_Read)(ANALOG_CHANNEL channel);
} TAnalog;

extern void TAnalog_Init(void);

typedef struct
{
	void (*Init)(void);
	void (*Uninit)(void);

    //bool (*WriteRead)(const SPI_CONFIGURATION& Configuration, byte* Write8, int WriteCount, byte* Read8, int ReadCount, int ReadStartOffset);
    //bool (*WriteRead16)(const SPI_CONFIGURATION& Configuration, ushort* Write16, int WriteCount, ushort* Read16, int ReadCount, int ReadStartOffset);
} TSpi;

extern void TSpi_Init(void);

/*typedef struct
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
*/
// 全局系统根
typedef struct
{
	void (*Init)(void);
	void (*Uninit)(void);
	
	int Clock;

	TBoot Boot;
	TCore Core;
	TMem Mem;
	TFlash Flash;
	TIO IO;
	TUsart Usart;
	TAnalog Analog;
	TSpi Spi;
	/*TI2c I2c;
	TPwm Pwm;
	TLcd Lcd;*/
} TSystem;

// 声明全局的Sys根对象
extern TSystem Sys;

// 使用何种模块的宏定义
#define using(module) Sys.module.Init = T##module##_Init;

#endif //_SYSTEM_H_
