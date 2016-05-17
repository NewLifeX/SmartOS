#ifndef _Buffer_H_
#define _Buffer_H_

/*
数据区设计原则：
1，固定数据区封装 Buffer	=> Object
2，变长数据区封装 Array		=> Buffer
3，自带初始缓冲区封装 ByteArray/String/TArray<T>	=> Array

赋值运算符原则：
1，赋值运算拷贝长度和数据，核心语义为拷贝一份数据区
2，左值长度大于等于右值时，左值长度变小
3，左值长度小于右值时，左值尝试SetLength扩容
4，如果扩容失败，调试版断言失败，发行版左值长度保持不变
*/

// 内存数据区。包装指针和长度
// 参考C#的Byte[]，主要实现对象与指针的相互转化、赋值、拷贝、设置、截取、比较等操作。
// 内部指针指向的内存和长度，都由外部传入，内部不会自动分配。
// 所有的进出拷贝都是针对内部指针和最大长度，不会自动扩容，除非子类继承扩展SetLength。
// 拷贝的原则是尽力而为，有多少可用空间就拷贝多少长度。
class Buffer : public Object
{
public:
	// 打包一个指针和长度指定的数据区
	Buffer(void* ptr, int len);
	// 不能用简写的模板，否则String可能命中自己的构造函数
	/*template<typename T, int N>
	Buffer(T (&arr)[N])
	{
		_Arr	= (char*)arr;
		_Length	= sizeof(arr);
	}
	template<typename T>
	Buffer(T (&obj))
	{
		_Arr	= (char*)&obj;
		_Length	= sizeof(obj);
	}*/
	// 拷贝构造函数。直接把指针和长度拿过来用
	Buffer(const Buffer& buf) = delete;
	// 对象mov操作，指针和长度归我，清空对方
	Buffer(Buffer&& rval);

	// 从另一个对象拷贝数据和长度，长度不足且扩容失败时报错
	Buffer& operator = (const Buffer& rhs);
	// 从指针拷贝，使用我的长度
	Buffer& operator = (const void* ptr);
	// 对象mov操作，指针和长度归我，清空对方
	Buffer& operator = (Buffer&& rval);

	// 拿出指针供外部使用
	inline byte* GetBuffer() { return (byte*)_Arr; }
	inline const byte* GetBuffer() const { return (byte*)_Arr; }
	inline int Length() const { return _Length; }

	// 设置数组长度。只能缩小不能扩大，子类可以扩展以实现自动扩容
	virtual bool SetLength(int len);
	//virtual void SetBuffer(void* ptr, int len);

    // 重载索引运算符[]，返回指定元素的第一个字节
    byte operator[](int i) const;
	// 支持 buf[i] = 0x36 的语法
    byte& operator[](int i);

	// 拷贝数据，默认-1长度表示当前长度
	virtual int Copy(int destIndex, const void* src, int len);
	// 把数据复制到目标缓冲区，默认-1长度表示当前长度
	virtual int CopyTo(int srcIndex, void* dest, int len) const;
	// 拷贝数据，默认-1长度表示两者最小长度
	virtual int Copy(int destIndex, const Buffer& src, int srcIndex, int len);
	int Copy(const Buffer& src, int destIndex = 0);

	// 用指定字节设置初始化一个区域
	int Set(byte item, int index, int len);
	void Clear(byte item = 0);

	// 截取一个子缓冲区，默认-1长度表示剩余全部
	//### 这里逻辑可以考虑修改为，当len大于内部长度时，直接用内部长度而不报错，方便应用层免去比较长度的啰嗦
	Buffer Sub(int index, int len);
	const Buffer Sub(int index, int len) const;

	// 显示十六进制数据，指定分隔字符和换行长度
	String& ToHex(String& str, char sep = 0, int newLine = 0) const;
	// 显示十六进制数据，指定分隔字符和换行长度
	String ToHex(char sep = 0, int newLine = 0) const;

	ushort	ToUInt16() const;
	uint	ToUInt32() const;
	UInt64	ToUInt64() const;
	void Write(ushort value, int index = 0);
	void Write(short value, int index = 0);
	void Write(uint value, int index = 0);
	void Write(int value, int index = 0);
	void Write(UInt64 value, int index = 0);

	// 输出对象的字符串表示方式
	virtual String& ToStr(String& str) const;

    explicit operator bool() const { return _Length > 0; }
    bool operator !() const { return _Length == 0; }
	friend bool operator == (const Buffer& bs1, const Buffer& bs2);
	friend bool operator == (const Buffer& bs1, const void* ptr);
	friend bool operator != (const Buffer& bs1, const Buffer& bs2);
	friend bool operator != (const Buffer& bs1, const void* ptr);

protected:
    char*	_Arr;		// 数据指针
	int		_Length;	// 长度

	void move(Buffer& rval);
};

#endif
