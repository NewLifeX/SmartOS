#ifndef _Dictionary_H_
#define _Dictionary_H_

// 字典。仅用于存储指针。
// 内置两个List用于存储键值集合，添加删除时对它们进行同步操作
class Dictionary
{
public:
    Dictionary(IComparer comparer = nullptr);

	int Count() const;
	const List& Keys() const;
	const List& Values() const;

	// 添加单个元素
    void Add(const void* key, void* value);

	// 删除指定元素
	void Remove(const void* key);

	void Clear();
	
	// 是否包含指定项
	bool ContainKey(const void* key) const;

	// 尝试获取值
	bool TryGetValue(const void* key, void*& value) const;

    // 重载索引运算符[]，返回指定元素的第一个
    void* operator[](const void* key) const;
    void*& operator[](const void* key);

	const String GetString(const void* key) const;
	
#if DEBUG
	static void Test();
#endif

private:
	List	_Keys;
	List	_Values;
};

#endif
