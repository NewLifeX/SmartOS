#ifndef __CAN_H__
#define __CAN_H__

// CAN类
class Can
{
public:
    typedef enum
    {
        Mode_Send = 0x00,        // 发送模式
        STD_Data = 0x01,    // 只接收标准数据帧
        STD_Remote = 0x02,  // 只接收标准远程帧
        STD = 0x03,         // 标准帧不会被过滤掉
        EXT_Data = 0x04,    // 只接收扩展数据帧
        EXT_Remote = 0x05,  // 只接收扩展远程帧
        EXT = 0x06,         // 扩展帧不会被过滤掉
        Mode_ALL = 0x07          // 接收所有类型
    }Mode_TypeDef;

    Mode_TypeDef Mode;  // 工作模式

    Can(CAN index = Can1, Mode_TypeDef mode = Mode_Send, int remap = 1);
    ~Can();

	void Open();
    void Send(byte* buf, uint len);

private:
	byte	_index;
    int		Remap;

    void*	_Tx;
    void*	_Rx;
	
	void OnOpen();
};

#endif
