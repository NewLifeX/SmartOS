#include "Sys.h"
#include "Port.h"
#include "NRF24L01.h"

byte RX_BUF[RX_PLOAD_WIDTH];		//接收数据缓存
byte TX_BUF[TX_PLOAD_WIDTH];		//发射数据缓存
/*发送  接受   地址*/
byte TX_ADDRESS[TX_ADR_WIDTH] = {0x34,0x43,0x10,0x10,0x01};  
byte RX_ADDRESS[RX_ADR_WIDTH] = {0x34,0x43,0x10,0x10,0x01}; 

//nRF2401 状态  供委托使用
enum nRF_state
{
	nrf_mode_rx,
	nrf_mode_tx,
	nrf_mode_free
}
nRF24L01_status=nrf_mode_free;
//byte nRF24L01_status;

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

NRF24L01::NRF24L01(int spi)
{
#ifdef STM32F10X
	#if Other_nRF_CSN
		Port::SetOutput(nRF2401_CSN, false);
	#else	
        Port::SetOutput(spi_nss[nRF2401_SPI], false);		
		if(nRF2401_SPI==SPI_3)
		{	//PA15是jtag接口中的一员 想要使用 必须开启remap
			RCC_APB2PeriphClockCmd( RCC_APB2Periph_AFIO, ENABLE);	
			GPIO_PinRemapConfig( GPIO_Remap_SWJ_JTAGDisable, ENABLE);
		}
	#endif  //Other_nRF_CSN
	#if us_nrf_ce
		Port::SetOutput(nRF2401_CE, false);
	#endif
	//中断引脚初始化
	//Sys.IO.OpenPort(nRF2401_IRQ_pin, GPIO_Mode_IN_FLOATING,  GPIO_Speed_10MHz , 0, GPIO_PuPd_DOWN );
    Port::SetInput(nRF2401_IRQ_pin, true);
	//中断引脚申请委托	
	Port::Register(nRF2401_IRQ_pin, nRF24L01_irq);
#else
	#if Other_nRF_CSN
		Port::SetOutput(nRF2401_CSN, false, Port::Speed_10MHz);
	#else	
		Port::SetOutput(spi_nss[nRF2401_SPI], false, Port::Speed_10MHz);

	#endif  //Other_nRF_CSN
	#if us_nrf_ce
		Port::SetOutput(nRF2401_CE, Port::Speed_10MHz);
	#endif
	//中断引脚初始化
	Port::SetInput(nRF2401_IRQ_pin, false, Port::Speed_10MHz , Port::PuPd_UP);
	//中断引脚申请委托	
	Port::Register(nRF2401_IRQ_pin, nRF24L01_irq);
#endif

    _spi = new Spi(spi);
}

/**
  * @brief   用于向NRF的寄存器中写入一串数据
  * @param   
  *		@arg reg : NRF的命令+寄存器地址
  *		@arg pBuf：存储了将要写入写寄存器数据的数组，外部定义
  * 	@arg bytes: pBuf的数据长度
  * @retval  NRF的status寄存器的状态
  */
byte NRF24L01::WriteBuf(byte reg ,byte *pBuf,byte bytes)
{
	byte status,byte_cnt;
	NRF_CE_LOW();
   	 /*置低CSN，使能SPI传输*/
	NRF_CSN_LOW();			
	 /*发送寄存器号*/	
	status = _spi->ReadWriteByte8(reg);
  	  /*向缓冲区写入数据*/
	for(byte_cnt=0;byte_cnt<bytes;byte_cnt++)
	//	SPI_NRF_RW(*pBuf++);	//写数据到缓冲区 	 
		_spi->ReadWriteByte8(*pBuf++);
	/*CSN拉高，完成*/
	NRF_CSN_HIGH();			
  	return (status);	//返回NRF24L01的状态 		
}

/**
  * @brief   用于向NRF的寄存器中写入一串数据
  * @param   
  *		@arg reg : NRF的命令+寄存器地址
  *		@arg pBuf：用于存储将被读出的寄存器数据的数组，外部定义
  * 	@arg bytes: pBuf的数据长度
  * @retval  NRF的status寄存器的状态
  */
byte NRF24L01::ReadBuf(byte reg,byte *pBuf,byte bytes)
{
 	byte status, byte_cnt;
	  NRF_CE_LOW();
	/*置低CSN，使能SPI传输*/
	NRF_CSN_LOW();
	/*发送寄存器号*/		
	status = _spi->ReadWriteByte8(reg); 
 	/*读取缓冲区数据*/
	for(byte_cnt=0;byte_cnt<bytes;byte_cnt++)		  
	  pBuf[byte_cnt] = _spi->ReadWriteByte8(NOP); //从NRF24L01读取数据  
	 /*CSN拉高，完成*/
	NRF_CSN_HIGH();	
 	return status;		//返回寄存器状态值
}

/**
  * @brief  主要用于NRF与MCU是否正常连接
  * @param  无
  * @retval SUCCESS/ERROR 连接正常/连接失败
  */
byte NRF24L01::Check(void)
{
	byte buf[5]={0xC2,0xC2,0xC2,0xC2,0xC2};
	byte buf1[5];
	byte i; 
	/*写入5个字节的地址.  */  
	WriteBuf(NRF_WRITE_REG+TX_ADDR,buf,5);
	/*读出写入的地址 */
	ReadBuf(TX_ADDR,buf1,5); 
	/*比较*/               
	for(i=0;i<5;i++)
	{
		if(buf1[i]!=0xC2)
		break;
	} 
	if(i==5)
		return SUCCESS ;        //MCU与NRF成功连接 
	else
		return ERROR ;        //MCU与NRF不正常连接
}

/**
  * @brief   用于从NRF特定的寄存器读出数据
  * @param   
  *		@arg reg:NRF的命令+寄存器地址
  * @retval  寄存器中的数据
  */
byte NRF24L01::ReadReg(byte reg)
{
 	byte reg_val;
	NRF_CE_LOW();
	/*置低CSN，使能SPI传输*/
 	NRF_CSN_LOW();
  	 /*发送寄存器号*/
	_spi->ReadWriteByte8(reg); 
	 /*读取寄存器的值 */
	reg_val =  _spi->ReadWriteByte8(NOP);
   	/*CSN拉高，完成*/
	NRF_CSN_HIGH();		
	return reg_val;
}	

/**
  * @brief   用于向NRF特定的寄存器写入数据
  * @param   
  *		@arg reg:NRF的命令+寄存器地址
  *		@arg dat:将要向寄存器写入的数据
  * @retval  NRF的status寄存器的状态
  */
byte NRF24L01::WriteReg(byte reg,byte dat)
{
 	byte status;
	 NRF_CE_LOW();
	/*置低CSN，使能SPI传输*/
    NRF_CSN_LOW();
	/*发送命令及寄存器号 */
	status = _spi->ReadWriteByte8(reg);
	 /*向寄存器写入数据*/
    _spi->ReadWriteByte8(dat); 
	/*CSN拉高，完成*/	   
  	NRF_CSN_HIGH();	
	/*返回状态寄存器的值*/
   	return(status);
}

/**
  * @brief  配置并进入接收模式
  * @param  无
  * @retval 无
  */
void NRF24L01::EnterReceive(void)
{
	NRF_CE_LOW();	
	WriteBuf(NRF_WRITE_REG+RX_ADDR_P0,RX_ADDRESS,RX_ADR_WIDTH);//写RX节点地址
	WriteReg(NRF_WRITE_REG+EN_AA,0x01);    //使能通道0的自动应答    
	WriteReg(NRF_WRITE_REG+EN_RXADDR,0x01);//使能通道0的接收地址    
	WriteReg(NRF_WRITE_REG+RF_CH,CHANAL);      //设置RF通信频率    
	WriteReg(NRF_WRITE_REG+RX_PW_P0,RX_PLOAD_WIDTH);//选择通道0的有效数据宽度      
    //WriteReg(NRF_WRITE_REG+RF_SETUP,0x0f); //设置TX发射参数,0db增益,2Mbps,低噪声增益开启  
	WriteReg(NRF_WRITE_REG+RF_SETUP,0x07);  //设置TX发射参数,0db增益,1Mbps,低噪声增益开启  
	WriteReg(NRF_WRITE_REG+CONFIG, 0x0f);  //配置基本工作模式的参数;  PWR_UP,  EN_CRC, 16BIT_CRC, 接收模式 
    /*CE拉高，进入接收模式*/	
	NRF_CE_HIGH();
	nRF24L01_status=nrf_mode_rx;
}    

/**
  * @brief  配置发送模式
  * @param  无
  * @retval 无
  */
void NRF24L01::EnterSend(void)
{  
	NRF_CE_LOW();
	WriteBuf(NRF_WRITE_REG+TX_ADDR,TX_ADDRESS,TX_ADR_WIDTH);    //写TX节点地址 
	WriteBuf(NRF_WRITE_REG+RX_ADDR_P0,RX_ADDRESS,RX_ADR_WIDTH); //设置TX节点地址,主要为了使能ACK   
	WriteReg(NRF_WRITE_REG+EN_AA,0x01);     //使能通道0的自动应答    
	WriteReg(NRF_WRITE_REG+EN_RXADDR,0x01); //使能通道0的接收地址  
	WriteReg(NRF_WRITE_REG+SETUP_RETR,0x1a);//设置自动重发间隔时间:500us + 86us;最大自动重发次数:10次
	WriteReg(NRF_WRITE_REG+RF_CH,CHANAL);       //设置RF通道为CHANAL
 // WriteReg(NRF_WRITE_REG+RF_SETUP,0x0f);  //设置TX发射参数,0db增益,2Mbps,低噪声增益开启   
	WriteReg(NRF_WRITE_REG+RF_SETUP,0x07);  //设置TX发射参数,0db增益,1Mbps,低噪声增益开启   
	WriteReg(NRF_WRITE_REG+CONFIG,0x0e);    //配置基本工作模式的参数;  PWR_UP,  EN_CRC,  16BIT_CRC,  发射模式,   开启所有中断
/*CE拉高，进入发送模式*/	
	NRF_CE_HIGH();
	nRF24L01_status=nrf_mode_tx;
  Sys.Sleep(500); //CE要拉高一段时间才进入发送模式
}

/*
此处有等待中断的 无限循环   注意       
需要修改
*/

/**
  * @brief   用于从NRF的接收缓冲区中读出数据
  * @param   
  *		@arg rxBuf ：用于接收该数据的数组，外部定义	
  * @retval 
  *		@arg 接收结果
  */
byte NRF24L01::Receive(byte *data)
{
	byte state; 
	/*等待接收中断*/
//	while(NRF_Read_IRQ()!=0); 
	NRF_CE_HIGH();	 //进入接收状态
	NRF_CE_LOW();  	 //进入待机状态
	/*读取status寄存器的值  */               
	state=ReadReg(STATUS);
	/* 清除中断标志*/      
	WriteReg(NRF_WRITE_REG+STATUS,state);
	/*判断是否接收到数据*/
	if(state&RX_DR)                                 //接收到数据
	{
	  ReadBuf(RD_RX_PLOAD, data, RX_PLOAD_WIDTH);//读取数据
	  WriteReg(FLUSH_RX,NOP);          //清除RX FIFO寄存器
	  return RX_DR; 
	}
	else    
		return ERROR;                    //没收到任何数据
}

/**
  * @brief   用于向NRF的发送缓冲区中写入数据
  * @param   
  *		@arg txBuf：存储了将要发送的数据的数组，外部定义	
  * @retval  发送结果，成功返回TXDS,失败返回MAXRT或ERROR
  */
byte NRF24L01::Send(byte* data)
{
	byte state;  
	 /*ce为低，进入待机模式1*/
	NRF_CE_LOW();
	/*写数据到TX BUF 最大 32个字节*/						
	WriteBuf(WR_TX_PLOAD, data, TX_PLOAD_WIDTH);
      /*CE为高，txbuf非空，发送数据包 */   
 	NRF_CE_HIGH();
	  /*等待发送完成中断 */                            
//	while(NRF_Read_IRQ()!=0); 	
	/*读取状态寄存器的值 */                              
	state = ReadReg(STATUS);
	 /*清除TX_DS或MAX_RT中断标志*/                  
	WriteReg(NRF_WRITE_REG+STATUS,state); 	
	WriteReg(FLUSH_TX,NOP);    //清除TX FIFO寄存器 
	 /*判断中断类型*/    
    if(state&MAX_RT)                     //达到最大重发次数
        return MAX_RT; 
    else if(state&TX_DS)                  //发送完成
        return TX_DS;
    else						  
        return ERROR;                 //其他原因发送失败
} 

//2401委托函数  未完成
void nRF24L01_irq(Pin pin, bool opk)
{
	byte a=3 ,b=5;
	(void)pin;
	(void)opk;
	switch(nRF24L01_status)
	{
        case nrf_mode_rx: a=b; break;
        case nrf_mode_tx: a+=b; break;
        case nrf_mode_free: a-=b; break;
	}
}
