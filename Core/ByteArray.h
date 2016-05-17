#ifndef _ByteArray_H_
#define _ByteArray_H_

// 字节数组
class ByteArray : public Array
{
public:
	explicit ByteArray(int length = 0);
	ByteArray(byte item, int length);
	// 因为使用外部指针，这里初始化时没必要分配内存造成浪费
	ByteArray(const void* data, int length, bool copy = false);
	ByteArray(void* data, int length, bool copy = false);
	explicit ByteArray(const Buffer& arr);
	ByteArray(const ByteArray& arr) = delete;
	ByteArray(ByteArray&& rval);
	//ByteArray(String& str);			// 直接引用数据缓冲区
	//ByteArray(const String& str);	// 不允许修改，拷贝

	ByteArray& operator = (const Buffer& rhs);
	ByteArray& operator = (const ByteArray& rhs);
	ByteArray& operator = (const void* p);
	ByteArray& operator = (ByteArray&& rval);

	// 重载等号运算符，使用外部指针、内部长度，用户自己注意安全
    //ByteArray& operator=(const void* data);

	// 保存到普通字节数组，首字节为长度
	int Load(const void* data, int maxsize = -1);
	// 从普通字节数据组加载，首字节为长度
	int Save(void* data, int maxsize = -1) const;

    //friend bool operator==(const ByteArray& bs1, const ByteArray& bs2);
    //friend bool operator!=(const ByteArray& bs1, const ByteArray& bs2);

protected:
	byte	Arr[0x40];	// 内部缓冲区

	virtual void* Alloc(int len);

	void move(ByteArray& rval);
};

#endif
