#ifndef __LinkMessage_H__
#define __LinkMessage_H__

#include "Kernel\Sys.h"

#include "Message\DataStore.h"
#include "Message\Json.h"

// 物联消息
struct LinkMessage
{
public:
	byte	Code : 6;	// 代码
	byte	Error : 1;	// 是否错误
	byte	Reply : 1;	// 是否响应
	byte	Seq;		// 序列号
	ushort	Length;		// 长度

	// 数据指针
	const void* Data() const { return (const void*)&this[1]; }
	const Buffer GetBuffer() const { return Buffer((void*)this, sizeof(this[0]) + Length); }
	const String GetString() const { return String((cstring)&this[1], Length); }

	void Init();

	// 在数据区上建立Json对象
	const Json Create() const;
	Json Create(int len);

	void Show(bool newline = false) const;

private:
};

#endif
