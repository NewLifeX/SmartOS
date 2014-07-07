#ifndef __NRF24L01_H__
#define __NRF24L01_H__



/********************  *************  ***********************/
/********************  *************  ***********************/


//使用哪个spi作为 nrf 通信口
#define 	nRF2401_SPI					SPI_1


//中断引脚
#define 	nRF2401_IRQ_pin		 PA1


//中断引脚检测
#define NRF_Read_IRQ()		  Sys.IO.Read(nRF2401_IRQ_pin)  


//是否使用非默认csn引脚
#define 	Other_nRF_CSN 			0

//是否使用CE引脚
#define		us_nrf_ce						0



//CE引脚操作
#if us_nrf_ce

	//CE引脚定义
	#define   nRF2401_CE					PE10
	#define 	NRF_CE_LOW()				Sys.IO.Write(nRF2401_CE,0)
	#define 	NRF_CE_HIGH()				Sys.IO.Write(nRF2401_CE,1)
	
#else

	#define 	NRF_CE_LOW()				
	#define 	NRF_CE_HIGH()				
#endif


//csn引脚操作
#if	!Other_nRF_CSN

			//一般情况   csn引脚用nss引脚
	#define  	NRF_CSN_LOW()				Sys.IO.Write(spi_nss[nRF2401_SPI],0)	
	#define  	NRF_CSN_HIGH()			Sys.IO.Write(spi_nss[nRF2401_SPI],1)	
						
			//自定义csn引脚
#else

	//CSN引脚定义
	#define   nRF2401_CSN						PE10
	#define 	NRF_CSN_LOW()					Sys.IO.Write(nRF2401_CSN,0)
	#define 	NRF_CSN_HIGH()				Sys.IO.Write(nRF2401_CSN,1)

#endif



//定义缓冲区大小  单位  byte
#define RX_PLOAD_WIDTH				5
#define TX_PLOAD_WIDTH				5

extern unsigned char RX_BUF[];		//接收数据缓存
extern unsigned char TX_BUF[];		//发射数据缓存



/********************  *************  ***********************/
/********************  *************  ***********************/


/*发送包大小*/
#define TX_ADR_WIDTH	5
#define RX_ADR_WIDTH	5


// 定义一个静态发送地址
/*初始地址到。c去设置*/
extern unsigned char TX_ADDRESS[];  		
extern unsigned char RX_ADDRESS[]; 


#define CHANAL 				40	//频道选择 


/********************  *************  ***********************/
/********************  *************  ***********************/


// SPI(nRF24L01) commands ,	NRF的SPI命令宏定义，详见NRF功能使用文档
#define NRF_READ_REG    0x00  // Define read command to register
#define NRF_WRITE_REG   0x20  // Define write command to register
#define RD_RX_PLOAD 0x61  // Define RX payload register address
#define WR_TX_PLOAD 0xA0  // Define TX payload register address
#define FLUSH_TX    0xE1  // Define flush TX register command
#define FLUSH_RX    0xE2  // Define flush RX register command
#define REUSE_TX_PL 0xE3  // Define reuse TX payload register command
#define NOP         0xFF  // Define No Operation, might be used to read status register

// SPI(nRF24L01) registers(addresses) ，NRF24L01 相关寄存器地址的宏定义
#define CONFIG      0x00  // 'Config' register address
#define EN_AA       0x01  // 'Enable Auto Acknowledgment' register address
#define EN_RXADDR   0x02  // 'Enabled RX addresses' register address
#define SETUP_AW    0x03  // 'Setup address width' register address
#define SETUP_RETR  0x04  // 'Setup Auto. Retrans' register address
#define RF_CH       0x05  // 'RF channel' register address
#define RF_SETUP    0x06  // 'RF setup' register address
#define STATUS      0x07  // 'Status' register address
#define OBSERVE_TX  0x08  // 'Observe TX' register address
#define CD          0x09  // 'Carrier Detect' register address
#define RX_ADDR_P0  0x0A  // 'RX address pipe0' register address
#define RX_ADDR_P1  0x0B  // 'RX address pipe1' register address
#define RX_ADDR_P2  0x0C  // 'RX address pipe2' register address
#define RX_ADDR_P3  0x0D  // 'RX address pipe3' register address
#define RX_ADDR_P4  0x0E  // 'RX address pipe4' register address
#define RX_ADDR_P5  0x0F  // 'RX address pipe5' register address
#define TX_ADDR     0x10  // 'TX address' register address
#define RX_PW_P0    0x11  // 'RX payload width, pipe0' register address
#define RX_PW_P1    0x12  // 'RX payload width, pipe1' register address
#define RX_PW_P2    0x13  // 'RX payload width, pipe2' register address
#define RX_PW_P3    0x14  // 'RX payload width, pipe3' register address
#define RX_PW_P4    0x15  // 'RX payload width, pipe4' register address
#define RX_PW_P5    0x16  // 'RX payload width, pipe5' register address
#define FIFO_STATUS 0x17  // 'FIFO Status Register' register address




/********************     中断相关    ***********************/
/********************  *************  ***********************/



#define MAX_RT      0x10 //达到最大重发次数中断标志位

#define TX_DS		0x20 //发送完成中断标志位	  // 

#define RX_DR		0x40 //接收到数据中断标志位



/********************  *************  ***********************/
/********************  *************  ***********************/


/*	此处函数原型不需要    直接使用Sys.nRF.xxxx();
//初始化		包含委托申请
void SPI_NRF_Init(void);
//检查是否有连接到硬件
byte NRF_Check(void);
//设置接收模式
void NRF_RX_Mode(void);
//接收数据
byte NRF_Rx_Dat(byte *rxbuf);
//设置发送模式
void NRF_TX_Mode(void);
//发送数据
byte NRF_Tx_Dat(byte *txbuf);
//2401委托函数
void nRF24L01_irq(Pin pin, bool opk);
*/


#endif
