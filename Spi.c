
#include "System.h"

#ifdef STM32F1XX
#include "stm32f10x_spi.h"
#else
	#include "stm32f0xx_spi.h"
	#include "Pin_STM32F0.h"
#endif
#include "Pin.h"



static const Pin g_Spi_Pins_Map[][4] = SPI_PINS_FULLREMAP;
#define IS_SPI



//#define SPI_NSS_PINS  {4, 28, 15} // PA4, PB12, PA15
//#define SPI_SCLK_PINS {5, 29, 19} // PA5, PB13, PB3
//#define SPI_MISO_PINS {6, 30, 20} // PA6, PB14, PB4
//#define SPI_MOSI_PINS {7, 31, 21} // PA7, PB15, PB5



//
// typedef struct TSpi_Def
//{
//	void (*Init)(struct TSpi_Def* this);
//	void (*Uninit)(struct TSpi_Def* this);
//    //bool (*WriteRead)(const SPI_CONFIGURATION& Configuration, byte* Write8, int WriteCount, byte* Read8, int ReadCount, int ReadStartOffset);
//    //bool (*WriteRead16)(const SPI_CONFIGURATION& Configuration, ushort* Write16, int WriteCount, ushort* Read16, int ReadCount, int ReadStartOffset);
//
//} TSpi;
//extern void TSpi_Init(TSpi* this);
//

// ????




//#define IS_REMAP(com) ((Usart_Remap >> com) & 0x01 == 0x01)




//void TSpi_GetPins(int com, Pin* txPin, Pin* rxPin)
//{

//	const Pin* p;

//    *rxPin = *txPin = P0;
//	
//	p = g_Uart_Pins;
//	if(IS_REMAP(com)) p = g_Spi_Pins_Map;

//	com = com << 2;
//	*txPin  = p[com];
//	*rxPin  = p[com + 1];
//}




void TSpi_Init(TSpi* this)
{
    this->Open  = TUsart_Open;
    this->Open2 = TUsart_Open2;
    this->Close = TUsart_Close;
    this->Write = TUsart_Write;
    this->Read  = TUsart_Read;
    this->Flush = TUsart_Flush;
	this->SetRemap = TUsart_SetRemap;
    this->Register = TUsart_Register;
}










