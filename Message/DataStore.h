#ifndef __DataStore_H__
#define __DataStore_H__

#include "Sys.h"
#include "Stream.h"

// 数据存储适配器
class DataStore
{
public:
	ByteArray	Data;	// 数据
	bool		Strict;	// 是否严格限制存储区，读写不许越界。默认true

	// 初始化
	DataStore();

	// 写入数据
	int Write(uint offset, ByteArray& bs);
	// 读取数据
	int Read(uint offset, ByteArray& bs);

	typedef bool (*Handler)(DataStore& ds, uint offset, uint size, ByteArray& bs);
	class Area
	{
	public:
		uint	Offset;
		uint	Size;

		Handler	Writing;	// 写入之前
		Handler Wrote;		// 写入之后
		Handler Read;		// 读取之前
		
		Area();
	};

	Area Areas[0x10];

	// 注册某一块区域的读写钩子函数
	void Register(uint offset, uint size, Handler writing, Handler wrote, Handler read);
};

#endif
