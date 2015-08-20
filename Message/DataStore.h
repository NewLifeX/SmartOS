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

	typedef bool (*Handler)(uint offset, uint size, int mode);
	// 注册某一块区域的读写钩子函数
	void Register(uint offset, uint size, Handler hook);
	
private:
	class Area
	{
	public:
		uint	Offset;
		uint	Size;

		Handler	Hook;
		
		Area();
		bool Contain(uint offset, uint size);
	};

	Area Areas[0x10];

	bool OnHook(uint offset, uint size, int mode);
};

#endif
