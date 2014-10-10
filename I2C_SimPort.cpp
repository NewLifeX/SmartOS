
#include "Port.h"
#include "Sys.h"




#define IIC1 	0
#define IIC2 	1
#define IIC3 	2


class I2C_SimPortPort
{
public:
    int Speed;  // 速度 单位 bps/s    好像最高400k
    int Retry;  // 等待重试次数，默认200
    int Error;  // 错误次数
	ushort Address;	// 7位地址或10位地址
	bool HasSecAddress;	// 设备是否有子地址  

	// 使用端口和最大速度初始化，因为需要分频，实际速度小于等于该速度
    I2C_SimPortPort(I2C_TypeDef* iic = I2C1, uint speedHz = 10000);
    virtual ~I2C_SimPortPort();

	void SetPin(Pin acl = P0, Pin sda = P0);
	void GetPin(Pin* acl = NULL, Pin* sda = NULL);

	void Open();
	void Close();
	
	void Write(byte id=0xffff, byte addr, byte * buf =NULL,int len=0x00);
	byte* Read(byte id=0xffff, byte addr, int len=0X00);
	
private:
    byte _index;		// IIC
	int _delay;			// 根据速度匹配的延时

	Pin Pins[2];
	OutputPort* SCL;
	OutputPort* SDA;
	
	void Start();
	void Stop();
	
	void Ack();
	byte ReadData();
	bool WriteData(byte dat);
	bool SetID(byte id, bool tx = true);
	bool WaitForEvent(uint event);
};



I2C_SimPortPort::I2C_SimPortPort(byte iic , uint speedHz )
{
	_index = iic;
	Pin pins[][2] =  I2C_PINS;
	Pins[0] = pins[_index][0];
	Pins[1] = pins[_index][1];
	
	Speed = speedHz;
	_delay = Sys.Clock/speedHz;
	Retry = 200;
	Error = 0;
	Address = 0x00;
}

I2C_SimPortPort::~I2C_SimPortPort()
{
	Close();
}

void I2C_SimPortPort::SetPin(Pin acl , Pin sda )
{
	Pins[0] = acl;
	Pins[1] = sda;
}

void I2C_SimPortPort::GetPin(Pin* acl , Pin* sda )
{
	*acl = Pins[0];
	*sda = Pins[1];
}

void I2C_SimPortPort::Open()
{
	// gpio  复用开漏输出
	SCL = new OutputPort(Pins[0], false, true);
	SDA = new OutputPort(Pins[1], false, true);

}

void I2C_SimPortPort::Close()
{
	if(SCL) delete SCL;
	SCL = NULL;

	if(SDA) delete SDA;
	SDA = NULL;
}
/*
sda		   ----____
scl		___-------____
*/
void I2C_SimPortPort::Start()
{
	*SDA=1;   //发送起始条件的数据信号
	__nop();
	__nop();
	__nop(); 
	*SCL=1;	//起始条件建立时间大于4.7us,延时
	Sys.Delay(_delay);   
	*SDA=0;     //发送起始信号
	Sys.Delay(_delay);      
	*SCL=0;    //钳住I2C总线，准备发送或接收数据
	__nop();
	__nop();
	__nop();
	__nop();
}

/*
sda		   ____----
scl		___-------____
*/
void I2C_SimPortPort::Stop()
{  
	*SDA=0;    //发送结束条件的数据信号
				//发送结束条件的时钟信号
	Sys.Delay(_delay);   
	*SCL=1;    //结束条件建立时间大于4μ
	Sys.Delay(_delay);        
	*SDA=1;    //发送I2C总线结束信号
	__nop();
	__nop();
	__nop();
	__nop();
}


void I2C_SimPortPort::Ack()
{
	*SDA=0;     
	__nop();
	__nop();
	__nop();
	__nop();
	__nop();        
	*SCL=1;					//时钟低电平周期大于4μ
	Sys.Delay(_delay);  
	*SCL=0;               //清时钟线，钳住I2C总线以便继续接收
	__nop();
	__nop();
	__nop();
	__nop();
}


byte I2C_SimPortPort::ReadData()
{
	byte retc=0; 
	*SDA=1;             // 开
	for(int BitCnt=0;BitCnt<8;BitCnt++)
	{
		Sys.Delay(_delay/2);          
        *SCL=0;       // 置时钟线为低，准备接收数据位
		Sys.Delay(_delay); 
        *SCL=1;       // 置时钟线为高使数据线上数据有效
		Sys.Delay(_delay/2);
        retc=retc<<1;
        if(SDA->ReadInput()==1)retc=retc+1; //读数据位,接收的数据位放入retc中
     }
	Sys.Delay(_delay/2);
	*SCL=0;   
	__nop();
	__nop();
	__nop();
  return(retc);
}


bool I2C_SimPortPort::WriteData(byte dat)
{
	for(int BitCnt=0;BitCnt<8;BitCnt++)  //要传送的数据长度为8位
    {
		if((c<<BitCnt)&0x80)SDA=1;   //判断发送位
			else  SDA=0;                
		__nop();
		*SCL=1;               //置时钟线为高，通知被控器开始接收数据位   
		Sys.Delay(delay);       
		*SCL=0; 
    }
	Sys.Delay(delay/2); 
    *SDA=1;               //8位发送完后释放数据线，准备接收应答位
	__nop();
	__nop();
	__nop();
	__nop();
	__nop();
    *SCL=1;
	Sys.Delay(delay/2);
    if(SDA->ReadInput()==1)ack=0;     
       else ack=1;        //判断是否接收到应答信号
    SCL=0;
	__nop();
	__nop();
	__nop();
	__nop();
}


bool I2C_SimPortPort::SetID(byte id, bool write)
{
}

void I2C_SimPortPort::Write(byte id, byte addr, byte * buf ,int len)
{
}



