
#ifndef __IRCODING_H__
#define __IRCODING_H__

#include "Kernel\Sys.h"
#include "..\Storage\Storage.h"

/*	
	红外编码的处理，
	读写Flash内的编码，与应用交互
	
	空调的红外编码长度典型值是200byte，暂且为每个编码留有256byte空间
	对比 _BlockSize 为编码分配空间。按照 block 为单位分配  方便操作。
	
	Config 配置区单独占用一个 bolock 其后编码依次放置
	每个编码头字节描述编码本身的长度  所以 ByteArray.GetLength() 可以休息了
*/

class IRcoding
{
private:
	Storage* _Medium;	// 存储区控制器
	uint _BlockSize;	// 存储区块大小
	uint _OrigAddr;		// 存储区首地址
public:	
	static ushort CodingSize; 
	byte _CodingBlock;	
	byte _CodingNum;	// 已存在有效编码个数
	// 此位置存在不对齐问题  需要注意
	
public:
	IRcoding(Storage* flash = nullptr, uint blockSize = 0);
	//~IRcoding();
	// 设置配置区所在位置
	void SetCfgAddr(uint addr);
	
	bool SaveCoding(byte index, const Buffer& bs);
	bool GetCoding(byte index, Buffer& bs);
	
};


#endif
