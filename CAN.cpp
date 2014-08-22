#include "Sys.h"
#include "Port.h"
#include "CAN.h"

#ifdef STM32F1
static const Pin g_CAN_Pins_Map[] =  CAN_PINS;
static const Pin g_CAN_Pins_Map2[] =  CAN_PINS_REMAP2;
static const Pin g_CAN_Pins_Map3[] =  CAN_PINS_REMAP3;
#endif

CAN::CAN(CAN_TypeDef* port, Mode_TypeDef mode, int remap)
{
    Port = port;
    Mode = mode;
    Remap = remap;

  	/*外设时钟设置*/
	//RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);
  	/*IO设置*/
#ifdef STM32F1
    if(port == CAN1)
    {
        if(remap == 1)
            GPIO_PinRemapConfig(GPIO_Remap1_CAN1, ENABLE);
        else if(remap == 2)
            GPIO_PinRemapConfig(GPIO_Remap2_CAN1, ENABLE);
    }
    else if(port == CAN2)
        GPIO_PinRemapConfig(GPIO_Remap_CAN2, ENABLE);

    const Pin* p = g_CAN_Pins_Map;
    if(remap == 2)
        p = g_CAN_Pins_Map2;
    else if(remap == 3)
        p = g_CAN_Pins_Map3;
    
    AlternatePort tx(p[0], false, 50);
    InputPort rx(p[1], false, 50, InputPort::PuPd_UP);
#endif

#ifdef STM32F1
    Interrupt.SetPriority(USB_LP_CAN1_RX0_IRQn, 0);
#elif defined(STM32F4)
    Interrupt.SetPriority(CAN1_RX0_IRQn, 0);
#endif

    /************************CAN通信参数设置**********************************/
    /*CAN寄存器初始化*/
    CAN_DeInit(Port);
    CAN_StructInit(&_can);
    /*CAN单元初始化*/
    _can.CAN_TTCM = DISABLE;			   //MCR-TTCM  时间触发通信模式使能
    _can.CAN_ABOM = Mode == Mode_Send ? ENABLE : DISABLE;			   //MCR-ABOM  自动离线管理 
    _can.CAN_AWUM = Mode == Mode_Send ? ENABLE : DISABLE;			   //MCR-AWUM  自动唤醒模式
    _can.CAN_NART = DISABLE;			   //MCR-NART  禁止报文自动重传	  DISABLE-自动重传
    _can.CAN_RFLM = DISABLE;			   //MCR-RFLM  接收FIFO 锁定模式  DISABLE-溢出时新报文会覆盖原有报文  
    _can.CAN_TXFP = DISABLE;			   //MCR-TXFP  发送FIFO优先级 DISABLE-优先级取决于报文标示符 
    _can.CAN_Mode = CAN_Mode_Normal;  //正常发送模式
    _can.CAN_SJW = CAN_SJW_1tq;		   //BTR-SJW 重新同步跳跃宽度 2个时间单元
    _can.CAN_BS1 = CAN_BS1_3tq;		   //BTR-TS1 时间段1 占用了6个时间单元
    _can.CAN_BS2 = CAN_BS2_2tq;		   //BTR-TS1 时间段2 占用了3个时间单元
    _can.CAN_Prescaler  = 6;		   ////BTR-BRP 波特率分频器  定义了时间单元的时间长度 36/(1+6+3)/4 = 0.8Mbps
    CAN_Init(Port, &_can);
    
    if(Mode == Mode_Send)
        _TxMsg = new CanTxMsg();
    else
    {
        _RxMsg = new CanRxMsg();

        CAN_FilterInitTypeDef  filter;

        /*CAN过滤器初始化*/
        filter.CAN_FilterNumber=0;						//过滤器组0
        filter.CAN_FilterMode=CAN_FilterMode_IdMask;	//工作在标识符屏蔽位模式
        filter.CAN_FilterScale=CAN_FilterScale_32bit;	//过滤器位宽为单个32位。
        /* 使能报文标示符过滤器按照标示符的内容进行比对过滤，扩展ID不是如下的就抛弃掉，是的话，会存入FIFO0。 */

        int slave_id = 1;
        switch(Mode)
        {
            case EXT_Data:
                //只接受扩展数据帧
                filter.CAN_FilterIdHigh   = ((slave_id<<3)&0xFFFF0000)>>16;
                filter.CAN_FilterIdLow   = ((slave_id<<3)|CAN_ID_EXT|CAN_RTR_DATA)&0xFFFF;
                filter.CAN_FilterMaskIdHigh  = 0xFFFF;
                filter.CAN_FilterMaskIdLow   = 0xFFFF;
                break;
            case EXT_Remote:
                //只接受扩展远程帧
                filter.CAN_FilterIdHigh   = ((slave_id<<3)&0xFFFF0000)>>16;
                filter.CAN_FilterIdLow   = ((slave_id<<3)|CAN_ID_EXT|CAN_RTR_REMOTE)&0xFFFF;
                filter.CAN_FilterMaskIdHigh  = 0xFFFF;
                filter.CAN_FilterMaskIdLow   = 0xFFFF;
                break;
            case STD_Remote:
                //只接受标准远程帧
                filter.CAN_FilterIdHigh   = ((slave_id<<21)&0xffff0000)>>16;
                filter.CAN_FilterIdLow   = ((slave_id<<21)|CAN_ID_STD|CAN_RTR_REMOTE)&0xffff;
                filter.CAN_FilterMaskIdHigh  = 0xFFFF;
                filter.CAN_FilterMaskIdLow   = 0xFFFF;
                break;
            case STD_Data:
                //只接受标准数据帧
                filter.CAN_FilterIdHigh   = ((slave_id<<21)&0xffff0000)>>16;
                filter.CAN_FilterIdLow   = ((slave_id<<21)|CAN_ID_STD|CAN_RTR_DATA)&0xffff;
                filter.CAN_FilterMaskIdHigh  = 0xFFFF;
                filter.CAN_FilterMaskIdLow   = 0xFFFF;
                break;
            case EXT:
                //扩展帧不会被过滤掉
                filter.CAN_FilterIdHigh   = ((slave_id<<3)&0xFFFF0000)>>16;
                filter.CAN_FilterIdLow   = ((slave_id<<3)|CAN_ID_EXT)&0xFFFF;
                filter.CAN_FilterMaskIdHigh  = 0xFFFF;
                filter.CAN_FilterMaskIdLow   = 0xFFFC;
                break;
            case STD:
                //标准帧不会被过滤掉
                filter.CAN_FilterIdHigh   = ((slave_id<<21)&0xffff0000)>>16;
                filter.CAN_FilterIdLow   = ((slave_id<<21)|CAN_ID_STD)&0xffff;
                filter.CAN_FilterMaskIdHigh  = 0xFFFF;
                filter.CAN_FilterMaskIdLow   = 0xFFFC;
                break;
            case Mode_ALL:
                //接收所有类型
                filter.CAN_FilterIdHigh   = 0x0000;
                filter.CAN_FilterIdLow   = 0x0000;
                filter.CAN_FilterMaskIdHigh  = 0x0000;
                filter.CAN_FilterMaskIdLow   = 0x0000;
        }

        filter.CAN_FilterFIFOAssignment=CAN_Filter_FIFO0 ;				//过滤器被关联到FIFO0
        filter.CAN_FilterActivation=ENABLE;			//使能过滤器
        CAN_FilterInit(&filter);
        /*CAN通信中断使能*/
        CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);
    }
}

void CAN::Send(byte* buf, uint len)
{
    switch(Mode)
    {
        case STD_Data:
        case STD_Remote:
        case STD:
            _TxMsg->StdId = 0;
            _TxMsg->IDE = CAN_ID_STD;
            break;
        case EXT_Data:
        case EXT_Remote:
        case EXT:
            _TxMsg->StdId = 0;
            _TxMsg->IDE = CAN_ID_EXT;
            break;
    }

    _TxMsg->RTR=CAN_RTR_DATA;				 //发送的是数据
    _TxMsg->DLC=2;							 //数据长度为2字节
    _TxMsg->Data[0] = 0x12;
    _TxMsg->Data[1] = 0x34;
    // 未完成
}
