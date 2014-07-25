#include "System.h"
#include "nrf24l01.h"

/********************************************************************************/
//NRF24L01寄存器操作命令
#define READ_REG_NRF        0x00  //读配置寄存器,低5位为寄存器地址
#define WRITE_REG_NRF       0x20  //写配置寄存器,低5位为寄存器地址
#define RD_RX_PLOAD     0x61  //读RX有效数据,1~32字节
#define WR_TX_PLOAD     0xA0  //写TX有效数据,1~32字节
#define FLUSH_TX        0xE1  //清除TX FIFO寄存器.发射模式下用
#define FLUSH_RX        0xE2  //清除RX FIFO寄存器.接收模式下用
#define REUSE_TX_PL     0xE3  //重新使用上一包数据,CE为高,数据包被不断发送.
#define NOP             0xFF  //空操作,可以用来读状态寄存器	 
/********************************************************************************/
//SPI(NRF24L01)寄存器地址
#define CONFIG          0x00  //配置收发状态，CRC校验模式及收发状态响应方式;
															//bit0:1接收模式,0发射模式;bit1:电选择;bit2:CRC模式;bit3:CRC使能;
                              //bit4:中断MAX_RT(达到最大重发次数中断)使能;bit5:中断TX_DS使能;bit6:中断RX_DR使能
#define EN_AA           0x01  //自动应答功能设置  bit0~5,对应通道0~5
#define EN_RXADDR       0x02  //接收信道使能,bit0~5,对应通道0~5
#define SETUP_AW        0x03  //设置地址宽度(所有数据通道):bit[1:0]     00,3字节;01,4字节;02,5字节;
#define SETUP_RETR      0x04  //建立自动重发;bit3:0,自动重发计数器;bit7:4,自动重发延时 250*x+86us
#define RF_CH           0x05  //RF通道,bit6:0,工作通道频率;
#define RF_SETUP        0x06  //RF寄存器;bit3:传输速率(0:1Mbps,1:2Mbps);bit2:1,发射功率;bit0:低噪声放大器增益
#define STATUS          0x07  //状态寄存器;bit0:TX FIFO满标志;bit3:1,接收数据通道号(最大:6);
															//bit4,达到最多次重发  bit5:数据发送完成中断;bit6:接收数据中断;
#define OBSERVE_TX      0x08  //发送检测寄存器,bit7:4,数据包丢失计数器;bit3:0,重发计数器
#define CD              0x09  //载波检测寄存器,bit0,载波检测;
#define RX_ADDR_P0      0x0A  //数据通道0接收地址,最大长度5个字节,低字节在前
#define RX_ADDR_P1      0x0B  //数据通道1接收地址,最大长度5个字节,低字节在前
#define RX_ADDR_P2      0x0C  //数据通道2接收地址,最低字节可设置,高字节,必须同RX_ADDR_P1[39:8]相等;
#define RX_ADDR_P3      0x0D  //数据通道3接收地址,最低字节可设置,高字节,必须同RX_ADDR_P1[39:8]相等;
#define RX_ADDR_P4      0x0E  //数据通道4接收地址,最低字节可设置,高字节,必须同RX_ADDR_P1[39:8]相等;
#define RX_ADDR_P5      0x0F  //数据通道5接收地址,最低字节可设置,高字节,必须同RX_ADDR_P1[39:8]相等;
#define TX_ADDR         0x10  //发送地址(低字节在前),ShockBurstTM模式下,RX_ADDR_P0与此地址相等
#define RX_PW_P0        0x11  //接收数据通道0有效数据宽度(1~32字节),设置为0则非法
#define RX_PW_P1        0x12  //接收数据通道1有效数据宽度(1~32字节),设置为0则非法
#define RX_PW_P2        0x13  //接收数据通道2有效数据宽度(1~32字节),设置为0则非法
#define RX_PW_P3        0x14  //接收数据通道3有效数据宽度(1~32字节),设置为0则非法
#define RX_PW_P4        0x15  //接收数据通道4有效数据宽度(1~32字节),设置为0则非法
#define RX_PW_P5        0x16  //接收数据通道5有效数据宽度(1~32字节),设置为0则非法
#define NRF_FIFO_STATUS 0x17  //FIFO状态寄存器;bit0,RX FIFO寄存器空标志;bit1,RX FIFO满标志;bit2,3,保留
                              //bit4,TX FIFO空标志;bit5,TX FIFO满标志;bit6,1,循环发送上一数据包.0,不循环;
															
#define MAX_TX  		0x10  //达到最大发送次数中断
#define TX_OK   		0x20  //TX发送完成中断
#define RX_OK   		0x40  //接收到数据中断	

/*发送  接受   地址*/
byte TX_ADDRESS[TX_ADR_WIDTH] = TX_Address;  
byte RX_ADDRESS[RX_ADR_WIDTH] = RX_Address; 

/********************************************************************************/

//nRF2401 状态  供委托使用
enum nRF_state
{
	nrf_mode_rx,
	nrf_mode_tx,
	nrf_mode_free
}
nRF24L01_status=nrf_mode_free;
/********************************************************************************/
//函数原型
byte NRF24L01_Read_Buf(byte reg,byte *pBuf,byte len);
byte NRF24L01_Write_Buf(byte reg, byte *pBuf, byte len);
byte NRF24L01_Write_Reg(byte reg,byte value);
byte NRF24L01_Read_Reg(byte reg);
//2401委托函数
void nRF24L01_irq(Pin pin, bool opk);


/*cs引脚*/
static const Pin spi_nss[3]=
{
	//一般选取 硬件  spi nss引脚   
	PA4		,								//spi1
	PB12	,								//spi2
	PA15	,								//spi3
};

//相关  spi  及引脚配置
void nRF_Init(void)
{
#ifdef STM32F10X
	#if Other_nRF_CSN
		Sys.IO.Open (nRF2401_CSN,  GPIO_Mode_Out_PP );
	#else	
		Sys.IO.Open (spi_nss[nRF2401_SPI],  GPIO_Mode_Out_PP );		
		if(nRF2401_SPI==SPI_3)
		{	//PA15是jtag接口中的一员 想要使用 必须开启remap
			RCC_APB2PeriphClockCmd( RCC_APB2Periph_AFIO, ENABLE);	
			GPIO_PinRemapConfig( GPIO_Remap_SWJ_JTAGDisable, ENABLE);
		}
	#endif  //Other_nRF_CSN
	#if us_nrf_ce
		Sys.IO.Open (nRF2401_CE,  GPIO_Mode_Out_PP );
	#endif
	//中断引脚初始化
	//Sys.IO.OpenPort(nRF2401_IRQ_pin, GPIO_Mode_IN_FLOATING,  GPIO_Speed_10MHz , 0, GPIO_PuPd_DOWN );
		Sys.IO.Open (nRF2401_IRQ_pin, GPIO_Mode_IN_FLOATING );
	//中断引脚申请委托	
		Sys.IO.Register(nRF2401_IRQ_pin,nRF24L01_irq);
#else
	#if Other_nRF_CSN
		Sys.IO.OpenPort(nRF2401_CSN, GPIO_Mode_OUT, GPIO_Speed_10MHz, GPIO_OType_PP,GPIO_PuPd_NOPULL);
	#else	
		Sys.IO.OpenPort(spi_nss[nRF2401_SPI], GPIO_Mode_OUT, GPIO_Speed_10MHz, GPIO_OType_PP,GPIO_PuPd_NOPULL);

	#endif  //Other_nRF_CSN
	#if us_nrf_ce
		Sys.IO.OpenPort(nRF2401_CE, GPIO_Mode_OUT, GPIO_Speed_10MHz, GPIO_OType_PP,GPIO_PuPd_NOPULL);
	#endif
	//中断引脚初始化
	Sys.IO.OpenPort(nRF2401_IRQ_pin, GPIO_Mode_IN,  GPIO_Speed_10MHz , GPIO_OType_PP, GPIO_PuPd_UP );
	//中断引脚申请委托	
	Sys.IO.Register(nRF2401_IRQ_pin,nRF24L01_irq);
#endif	
	Sys.Spi.Open(nRF2401_SPI);	
	
	NRF24L01_Write_Reg(FLUSH_RX,0xff);//清除RX FIFO寄存器 
	NRF24L01_Write_Reg(FLUSH_TX,0xff);//清除RX FIFO寄存器 
}

//检测24L01是否存在
//返回值:0，成功;1，失败	
byte NRF24L01_Check(void)
{
	byte buf[5]={0XA5,0XA5,0XA5,0XA5,0XA5};
	byte i;
	Sys.Spi.SetSpeed(nRF2401_SPI,SPI_BaudRatePrescaler_4); //spi速度为9Mhz（24L01的最大SPI时钟为10Mhz）   	 
	NRF24L01_Write_Buf(WRITE_REG_NRF+TX_ADDR,buf,5);//写入5个字节的地址.	
	NRF24L01_Read_Buf(TX_ADDR,buf,5); //读出写入的地址  
	for(i=0;i<5;i++)if(buf[i]!=0XA5)break;	 							   
	if(i!=5)return 1;//检测24L01错误	
	return 0;		 //检测到24L01
}	

//启动NRF24L01发送一次数据
//txbuf:待发送数据首地址
//返回值:发送完成状况
byte NRF24L01_TxPacket(byte *txbuf)
{
	byte sta;
 	Sys.Spi.SetSpeed(nRF2401_SPI,SPI_BaudRatePrescaler_16);//spi速度为9Mhz（24L01的最大SPI时钟为10Mhz）   
	NRF_CE_LOW();
  NRF24L01_Write_Buf(WR_TX_PLOAD,txbuf,TX_PLOAD_WIDTH);//写数据到TX BUF  32个字节
 	NRF_CE_HIGH();					//启动发送	   
//	while(NRF24L01_IRQ!=0);//等待发送完成
	sta=NRF24L01_Read_Reg(STATUS);  //读取状态寄存器的值	   
	NRF24L01_Write_Reg(WRITE_REG_NRF+STATUS,sta); //清除TX_DS或MAX_RT中断标志
	if(sta&MAX_TX)//达到最大重发次数
	{
		NRF24L01_Write_Reg(FLUSH_TX,0xff);//清除TX FIFO寄存器 
		return MAX_TX; 
	}
	if(sta&TX_OK)//发送完成
	{
		return TX_OK;
	}
	return 0xff;//其他原因发送失败
}

//启动NRF24L01发送一次数据
//txbuf:待发送数据首地址
//返回值:0，接收完成；其他，错误代码
byte NRF24L01_RxPacket(byte *rxbuf)
{
	byte sta;		    							   
	Sys.Spi.SetSpeed(nRF2401_SPI,SPI_BaudRatePrescaler_16); //spi速度为9Mhz（24L01的最大SPI时钟为10Mhz）   
	sta=NRF24L01_Read_Reg(STATUS);  //读取状态寄存器的值    	 
	NRF24L01_Write_Reg(WRITE_REG_NRF+STATUS,sta); //清除TX_DS或MAX_RT中断标志
	if(sta&RX_OK)//接收到数据
	{
		NRF24L01_Read_Buf(RD_RX_PLOAD,rxbuf,RX_PLOAD_WIDTH);//读取数据
		NRF24L01_Write_Reg(FLUSH_RX,0xff);//清除RX FIFO寄存器 
		return 0; 
	}	   
	return 1;//没收到任何数据
}		

//该函数初始化NRF24L01到RX模式
//设置RX地址,写RX数据宽度,选择RF频道,波特率和LNA HCURR
//当CE变高后,即进入RX模式,并可以接收数据了		   
void NRF24L01_RX_Mode(void)
{
		NRF_CE_LOW();	  
  	NRF24L01_Write_Buf(WRITE_REG_NRF+RX_ADDR_P0,RX_ADDRESS,RX_ADR_WIDTH);//写RX节点地址
	
		NRF24L01_Write_Reg(WRITE_REG_NRF+SETUP_AW,0x02);	//设置接收地址长度为5  **
  	NRF24L01_Write_Reg(WRITE_REG_NRF+EN_AA,0x01);    //使能通道0的自动应答    
  	NRF24L01_Write_Reg(WRITE_REG_NRF+EN_RXADDR,0x01);//使能通道0的接收地址  	 
  	NRF24L01_Write_Reg(WRITE_REG_NRF+RF_CH,CHANAL);	     //设置RF通信频率		  
  	NRF24L01_Write_Reg(WRITE_REG_NRF+RX_PW_P0,RX_PLOAD_WIDTH);//选择通道0的有效数据宽度 	    
  	NRF24L01_Write_Reg(WRITE_REG_NRF+RF_SETUP,0x07); 
  	NRF24L01_Write_Reg(WRITE_REG_NRF+CONFIG, 0x0f);//配置基本工作模式的参数;PWR_UP,EN_CRC,16BIT_CRC,接收模式 
  	NRF_CE_HIGH(); //CE为高,进入接收模式 
}				

//该函数初始化NRF24L01到TX模式
//设置TX地址,写TX数据宽度,设置RX自动应答的地址,填充TX发送数据,选择RF频道,波特率和LNA HCURR
//PWR_UP,CRC使能
//当CE变高后,即进入RX模式,并可以接收数据了		   
//CE为高大于10us,则启动发送.	 
void NRF24L01_TX_Mode(void)
{									
		NRF_CE_LOW();	     
  	NRF24L01_Write_Buf(WRITE_REG_NRF+TX_ADDR,(u8*)TX_ADDRESS,TX_ADR_WIDTH);//写TX节点地址 
  	NRF24L01_Write_Buf(WRITE_REG_NRF+RX_ADDR_P0,(u8*)RX_ADDRESS,RX_ADR_WIDTH); //设置TX节点地址,主要为了使能ACK	  
	
		NRF24L01_Write_Reg(WRITE_REG_NRF+SETUP_AW,0x02);	//设置接收地址长度为5  **
  	NRF24L01_Write_Reg(WRITE_REG_NRF+EN_AA,0x01);     //使能通道0的自动应答    
  	NRF24L01_Write_Reg(WRITE_REG_NRF+EN_RXADDR,0x01); //使能通道0的接收地址  
  	NRF24L01_Write_Reg(WRITE_REG_NRF+SETUP_RETR,0x1a);//设置自动重发间隔时间:500us + 86us;最大自动重发次数:10次
  	NRF24L01_Write_Reg(WRITE_REG_NRF+RF_CH,CHANAL);       //设置RF通道
  	NRF24L01_Write_Reg(WRITE_REG_NRF+RF_SETUP,0x07);  
  	NRF24L01_Write_Reg(WRITE_REG_NRF+CONFIG,0x0e);    //配置基本工作模式的参数;PWR_UP,EN_CRC,16BIT_CRC,接收模式,开启所有中断
		NRF_CE_HIGH();//CE为高,10us后启动发送
		Sys.Delay(1000);
}		  

/****************************************************************/

//SPI写寄存器
//reg:指定寄存器地址
//value:写入的值
byte NRF24L01_Write_Reg(byte reg,byte value)
{
	byte status;	
  NRF_CSN_LOW();                   //使能SPI传输
  status =Sys.Spi.WriteReadByte8(nRF2401_SPI,reg);//发送寄存器号 
  Sys.Spi.WriteReadByte8(nRF2401_SPI,value);      //写入寄存器的值
  NRF_CSN_HIGH();                 //禁止SPI传输	   
  return(status);       			//返回状态值
}

//读取SPI寄存器值
//reg:要读的寄存器
byte NRF24L01_Read_Reg(byte reg)
{
	byte reg_val;	    
 	NRF_CSN_LOW();          //使能SPI传输		
  Sys.Spi.WriteReadByte8(nRF2401_SPI,reg);   //发送寄存器号
  reg_val=Sys.Spi.WriteReadByte8(nRF2401_SPI,0XFF);//读取寄存器内容
  NRF_CSN_HIGH();          //禁止SPI传输		    
  return(reg_val);           //返回状态值
}	

//在指定位置读出指定长度的数据
//reg:寄存器(位置)
//*pBuf:数据指针
//len:数据长度
//返回值,此次读到的状态寄存器值 
byte NRF24L01_Read_Buf(byte reg,byte *pBuf,byte len)
{
	byte status,u8_ctr;	       
  NRF_CSN_LOW();           //使能SPI传输
  status=Sys.Spi.WriteReadByte8(nRF2401_SPI,reg);//发送寄存器值(位置),并读取状态值   	   
 	for(u8_ctr=0;u8_ctr<len;u8_ctr++)pBuf[u8_ctr]=Sys.Spi.WriteReadByte8(nRF2401_SPI,0XFF);//读出数据
  NRF_CSN_HIGH();       //关闭SPI传输
  return status;        //返回读到的状态值
}

//在指定位置写指定长度的数据
//reg:寄存器(位置)
//*pBuf:数据指针
//len:数据长度
//返回值,此次读到的状态寄存器值
byte NRF24L01_Write_Buf(byte reg, byte *pBuf, byte len)
{
	byte status,u8_ctr;	   
	NRF_CSN_LOW();	          //使能SPI传输
  status =Sys.Spi.WriteReadByte8(nRF2401_SPI,reg);//发送寄存器值(位置),并读取状态值
  for(u8_ctr=0; u8_ctr<len; u8_ctr++)Sys.Spi.WriteReadByte8(nRF2401_SPI,*pBuf++); //写入数据	 
  NRF_CSN_HIGH();       //关闭SPI传输
  return status;          //返回读到的状态值
}		

/****************************************************************/

//2401委托函数  未完成
void nRF24L01_irq(Pin pin, bool opk)
{
	(void)pin;
	(void)opk;
	nRF24L01_status=nrf_mode_tx;
//	switch(nRF24L01_status)
//	{
//		case 	nrf_mode_rx		:	a=b	;		
//				break;
//		case	 nrf_mode_tx	:	a+=b;		
//				break;
//		case	 nrf_mode_free	:	a-=b;		
//				break;
//	}
}

//初始化nRF函数接口
void TnRF_Init(TnRF* this)
{
    this->Open  	= nRF_Init;
    this->Check 	= NRF24L01_Check;
    this->RX_Mode = NRF24L01_RX_Mode;
    this->TX_Mode = NRF24L01_TX_Mode;
    this->Rx_Dat  = NRF24L01_RxPacket;
    this->Tx_Dat 	= NRF24L01_TxPacket;
//	Execute(Spi);
}
